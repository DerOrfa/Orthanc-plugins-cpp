#include "OrthancPluginCppWrapper.h"
#include <openssl/md5.h>
#include <string>
#include <memory>
#include <jsoncpp/json/reader.h>
#include <regex>

#include "patientnamemapping.hpp"
#include "tagprocessorlist.hpp"
#include "dicomhandle.hpp"

std::unique_ptr<PatientNameMapping> patient_name_map;
std::unique_ptr<TagProcessorList> tag_processor_list;

struct OrthancPluginString{
	char *begin,*end;
	explicit OrthancPluginString(char *p):begin(p){
		end=begin+strlen(begin);
	}
	~OrthancPluginString(){
		OrthancPluginFreeString(OrthancPlugins::GetGlobalContext(),begin);
	}
};

bool isSubjectID(const std::string& text, std::string &id, std::string &checksum){
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

bool checkSubjectID(const std::string& wholeID){
	std::string id,checksum;

	// if its no subjectID ignore it
	if(!isSubjectID(wholeID,id,checksum))
		return true;

	//check given checksum against computed md5
	unsigned char md[MD5_DIGEST_LENGTH];
	MD5(reinterpret_cast<const unsigned char *>(id.c_str()), 5, md);

	//if MD5 checks out, we're done here
	if((md[1]==checksum[0] && md[2]==checksum[1]) || (md[3]==checksum[0] && md[4]==checksum[1]))
		return true;

	//last resort, check for known good IDs
	if(patient_name_map->knownValue(wholeID))
		return true;

	OrthancPlugins::LogWarning(wholeID+ " was not found in list of known PatientIDs");
	return false;
}

bool MapPatient(DicomHandle& dcmfile){
	bool good=true;
	const auto patName = dcmfile.findString(DcmTagKey(0x0010, 0x0010));
	const auto patID = dcmfile.findString(DcmTagKey(0x0010, 0x0020));
	auto found_patName=patient_name_map->lookup(patName);
	auto found_patID=patient_name_map->lookup(patID);

	if(!found_patName.empty()){
		good &= dcmfile.replaceString(DcmTagKey(0x0010, 0x0010),found_patName);
	}

	if(!found_patID.empty()){
		good &= dcmfile.replaceString(DcmTagKey(0x0010, 0x0020),found_patID);
	}

	return good;
}


int32_t instanceFilter(const OrthancPluginDicomInstance *instance){
	static const std::unique_ptr<Json::CharReader> reader(Json::CharReaderBuilder().newCharReader());
	OrthancPluginString string(OrthancPluginGetInstanceSimplifiedJson(OrthancPlugins::GetGlobalContext(), instance));
	Json::Value root;
	std::string errs;

	reader->parse(string.begin,string.end,&root,&errs);
	if(root.isMember("PatientID") && !checkSubjectID(root["PatientID"].asString()))
		return -1; // this will be interpreted as corrupt file by orthanc an thus the scp-store will be rejected

	if(root.isMember("PatientName") && !checkSubjectID(root["PatientName"].asString()))
			return -1; // this will be interpreted as corrupt file by orthanc an thus the scp-store will be rejected

	return 1; //0 to discard the instance, 1 to store the instance, -1 if error.
}

OrthancPluginErrorCode transcoder(OrthancPluginMemoryBuffer *target, const void *buffer, uint64_t size, const char *const *allowedSyntaxes, uint32_t countSyntaxes, uint8_t allowNewSopInstanceUid)
{
	DicomHandle dcmfile(buffer,size);

	if(!dcmfile.valid){
		OrthancPlugins::LogError("Instance filter failed to load dicom data");
		return OrthancPluginErrorCode_BadFileFormat;
	}

	if(!MapPatient(dcmfile))	{
		OrthancPlugins::LogError("PatientID mapping failed");
		return OrthancPluginErrorCode_BadFileFormat;
	}

	tag_processor_list->FixTags(dcmfile.getDataset());

	if(dcmfile.SaveToMemoryBuffer(target))
		return OrthancPluginErrorCode::OrthancPluginErrorCode_Success; //0 if success (i.e. image successfully target and stored into "target"), or the error code if failure.
	else
		return OrthancPluginErrorCode_BadFileFormat;
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

	//setup up PatientName mapping
	OrthancPlugins::OrthancConfiguration patient_map_cfg,tag_processing_cfg;
	OrthancPlugins::OrthancConfiguration().GetSection(patient_map_cfg,"PatientIDMap");
	patient_name_map.reset(new PatientNameMapping(patient_map_cfg));

	//setup up tag processing mapping
	OrthancPlugins::OrthancConfiguration().GetSection(tag_processing_cfg,"ProcessTags");
	tag_processor_list.reset(new TagProcessorList(tag_processing_cfg));

	//setting up filter for incoming instances (this does not change data, only accepts or rejects them)
	OrthancPluginRegisterIncomingDicomInstanceFilter(c,instanceFilter);

	//register transcoder (this *does* change incoming data)
	OrthancPluginRegisterTranscoderCallback(c,transcoder);

	return 0;
}

ORTHANC_PLUGINS_API void OrthancPluginFinalize(){
	patient_name_map.release();
	tag_processor_list.release();
}
ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
{
	return "dicom instance filter";
}
ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion(){return "0.0";}
}