#include "OrthancPluginCppWrapper.h"
#include <openssl/md5.h>
#include <string>
#include <memory>
#include <jsoncpp/json/reader.h>
#include <regex>
#include <dcmtk/dcmdata/dcdict.h>
#include <boost/filesystem.hpp>

using namespace boost::filesystem;

struct OrthancPluginString{
	char *begin,*end;
	explicit OrthancPluginString(char *p):begin(p){
		end=begin+strlen(begin);
	}
	~OrthancPluginString(){
		OrthancPluginFreeString(OrthancPlugins::GetGlobalContext(),begin);
	}
};

class DicomMapping {
	path filename;
	size_t relevant_chars=0;
	char terminator=0;
	bool reverse=false;
	std::chrono::time_point<std::chrono::system_clock> last_update;
	std::map<std::string, std::string> map;
	std::set<std::string> values;

	void update(){
		std::ifstream in(filename.c_str());
		in.exceptions(std::ifstream::badbit);
		map.clear();
		OrthancPlugins::LogError(std::string("(Re)loading patient name mapping from ") + filename.native());
		while(in.good()){
			std::string buffer;
			std::getline(in,buffer);
			if(buffer.empty())
				continue;
			auto terminator_at=buffer.find(terminator);
			if(terminator_at==0 || terminator_at>=buffer.size()){
				OrthancPlugins::LogError(std::string("No terminator found for patient mapping entry \"") + buffer + "\" ignoring it");
				continue;
			}
			auto mapping=std::make_pair(buffer.substr(0,terminator_at),buffer.substr(terminator_at+1));
			if(reverse) {
				map[mapping.second] = mapping.first;
				values.insert(mapping.first);
			} else {
				map.insert(mapping);
				values.insert(mapping.second);
			}
		}
		last_update=std::chrono::system_clock::now();
	}


public:
	DicomMapping()=default;
	DicomMapping(path filename_, size_t relevant_chars_, char terminator_, bool reverse_):
		filename(std::move(filename_)),relevant_chars(relevant_chars_),terminator(terminator_),reverse(reverse_)
	{
		update();
	}

	std::string lookup(std::string org){
		if(relevant_chars && org.size()>relevant_chars) {
			auto inner = lookup(org.substr(0, relevant_chars));
			if(inner.empty())
				return {};
			else
				return org.replace(0, relevant_chars, inner);
		} else {
			if (std::chrono::system_clock::now() - last_update > std::chrono::minutes(10)) {
				try { update(); }
				catch (const std::ios_base::failure &fail) {
					OrthancPlugins::LogError(std::string("Failed to update patient mapping from ") + filename.native() + "(" + fail.what() + ")");
				}
			}
			auto found = map.find(org);
			if (found != map.end())
				return found->second;
			else
				return {};
		}
	}
	bool knownValue(std::string value){
		return values.find(value)!=values.end();
	}
} patient_name_map;

/**
 * Registers patient name mapping based on the configuration.
 * This expects an JsonObject of the following format in the configuration
 * \code
 * "PatientIDMap" : {
 *   "File" :  "prob_map.txt",
 *   "Terminator" : "|",
 *   "Reverse" : true
 * },
 * \endcode
 * "file" is expected with lines "<oldname><terminator><newname>"
 * the mapping will be reversed if "Reverse" is true
 */
void RegisterPatientNameMapping(const OrthancPlugins::OrthancConfiguration &map){
	const path map_file=map.GetStringValue("File","");
	const auto terminator=map.GetStringValue("Terminator","|");
	const auto relevant_chars = map.GetIntegerValue("RelevantChars",4);
	const auto reverse=map.GetBooleanValue("Reverse",false);

	if(!exists(map_file)) {
		OrthancPlugins::LogError(std::string("Patient name map file \"") + map_file.native() + "\" does not exist");
		ORTHANC_PLUGINS_THROW_EXCEPTION(InexistentFile);
	}
	patient_name_map=DicomMapping(map_file,relevant_chars,terminator[0],reverse);
}

bool isSubjectID(const std::string& text, std::string id, std::string checksum){
	std::smatch result;
	static std::regex isPatientID(
		"([0-9]{5})\\.([0-9a-f]{2}).*",
		std::regex_constants::ECMAScript|std::regex_constants::optimize|std::regex_constants::icase
	);
	if(std::regex_match(text,result,isPatientID)){
		id=result[1],checksum=result[2];
		return true;
	}
	return false;
}

bool checkSubjectID(const std::string& id,const std::string& checksum){
	//check given checksum against computed md5
	unsigned char md[MD5_DIGEST_LENGTH];
	MD5(reinterpret_cast<const unsigned char *>(id.c_str()), 5, md);

	return (md[1]==checksum[0] && md[2]==checksum[1]) || (md[3]==checksum[0] && md[4]==checksum[1]);
}

int32_t instanceFilter(const OrthancPluginDicomInstance *instance){
	static const std::unique_ptr<Json::CharReader> reader(Json::CharReaderBuilder().newCharReader());
	OrthancPluginString string(OrthancPluginGetInstanceSimplifiedJson(OrthancPlugins::GetGlobalContext(), instance));
	Json::Value root;
	std::string errs,id,checkSum;

	reader->parse(string.begin,string.end,&root,&errs);
	if(root.isMember("PatientID") && isSubjectID(root["PatientID"].asString(),id,checkSum)){ // if "PatientID" is a subject id
		if(checkSubjectID(id,checkSum))return 1; //all good checksum checks out
		else if(patient_name_map.knownValue(root["PatientID"].asString()))return 1; //its a known good id
		else {
			OrthancPlugins::LogWarning(root["PatientID"].asString()+ " was not found in list of known PatientIDs");
			return -1; // this will be interpreted as corrupt file by orthanc an thus the scp-store will be rejected
		}
	} else if(root.isMember("PatientName") && isSubjectID(root["PatientName"].asString(),id,checkSum)) {
		if(checkSubjectID(id,checkSum))return 1; //all good checksum checks out
		else if(patient_name_map.knownValue(root["PatientName"].asString()))return 1; //its a known good id
		else {
			OrthancPlugins::LogWarning(root["PatientName"].asString()+ " was not found in list of known PatientNames");
			return -1; // this will be interpreted as corrupt file by orthanc an thus the scp-store will be rejected
		}
	}
	return 1; //0 to discard the instance, 1 to store the instance, -1 if error.
}

extern "C"
{
ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
{
	OrthancPlugins::SetGlobalContext(c);

	/* Check the version of the Orthanc core */
	if (OrthancPluginCheckVersion(c) == 0)
	{
		OrthancPlugins::ReportMinimalOrthancVersion(ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
													ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
													ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
		return -1;
	}

	OrthancPlugins::OrthancConfiguration patient_map;
	OrthancPlugins::OrthancConfiguration().GetSection(patient_map,"PatientIDMap");
	RegisterPatientNameMapping(patient_map);

	OrthancPluginRegisterIncomingDicomInstanceFilter(c,instanceFilter);

	return 0;
}

ORTHANC_PLUGINS_API void OrthancPluginFinalize(){}
ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
{
	return "dicom instance filter";
}
ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion(){return "0.0";}
}