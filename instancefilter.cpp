#include "OrthancPluginCppWrapper.h"
#include <openssl/md5.h>
#include <string>
#include <memory>
#include <jsoncpp/json/reader.h>
#include <regex>
#include <dcmtk/dcmdata/dcdict.h>

struct OrthancPluginString{
	char *begin,*end;
	explicit OrthancPluginString(char *p):begin(p){
		end=begin+strlen(begin);
	}
	~OrthancPluginString(){
		OrthancPluginFreeString(OrthancPlugins::GetGlobalContext(),begin);
	}
};

bool checkPatientID(const std::string& id){
	unsigned char md[MD5_DIGEST_LENGTH];
	static std::regex isPatientID(
		"([0-9]{5})\\.([0-9a-f]{2}).*",
		std::regex_constants::ECMAScript|std::regex_constants::optimize|std::regex_constants::icase
	);
	std::smatch result;
	if(std::regex_match(id,result,isPatientID)){
		std::string _id=result[1],checksum=result[2];
		MD5(reinterpret_cast<const unsigned char *>(_id.c_str()), 5, md);

		bool valid=
			(md[1]==checksum[0] && md[2]==checksum[1]) ||
			(md[3]==checksum[0] && md[4]==checksum[1])
		;
		if(valid)
			return true;
		else{
			printf("Digest ");
			for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
				printf("%02x", md[i]);
			printf(" failed\n");
			return false;
		}
	} else //let all "not-patientID" through
		return true;
}

int32_t instanceFilter(const OrthancPluginDicomInstance *instance){
	static const std::unique_ptr<Json::CharReader> reader(Json::CharReaderBuilder().newCharReader());
	OrthancPluginString string(OrthancPluginGetInstanceSimplifiedJson(OrthancPlugins::GetGlobalContext(), instance));
	Json::Value root;
	std::string errs;
	reader->parse(string.begin,string.end,&root,&errs);
	if(root.isMember("PatientID") && !checkPatientID(root["PatientID"].asString()))
		return -1; // this will be interpreted as corrupt file by othanc an thus the scp-store will be rejected
	if(root.isMember("PatientName") && !checkPatientID(root["PatientName"].asString()))
		return -1; // this will be interpreted as corrupt file by othanc an thus the scp-store will be rejected

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