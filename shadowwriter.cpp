#include "OrthancPluginCppWrapper.h"
#include <boost/filesystem.hpp>
#include <string>
#include <future>
#include <fcntl.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>

namespace fs = boost::filesystem;

static fs::path sroot,oroot;

fs::path GetOPath(const std::string& uuid)
{
	fs::path path = oroot;

	path /= std::string(&uuid[0], &uuid[2]);
	path /= std::string(&uuid[2], &uuid[4]);
	path /= uuid;

#if BOOST_HAS_FILESYSTEM_V3 == 1
	path.make_preferred();
#endif
	return path;
}
std::string find(DcmSequenceOfItems &dcm,DcmTagKey key){
	DcmStack resultStack;
	dcm.search(key, resultStack);
	if(resultStack.empty())
		return {};
	auto element=dynamic_cast<DcmElement*>(resultStack.top());
	if(element == nullptr)
		return {};
	OFString ret;
	element->getOFString(ret,0);
	return ret.c_str();
}
fs::path GetSPath(const void* buffer, size_t size)
{
	DcmInputBufferStream is;
	if (size > 0) {
		is.setBuffer(buffer, size);
	}
	is.setEos();

	DcmFileFormat dcm;

	dcm.transferInit();

	if (dcm.read(is, EXS_Unknown, EGL_noChange, size).good())
	{
		dcm.loadAllDataIntoMemory();
		dcm.transferEnd();
		auto PatientName = find(dcm,DcmTagKey(0x0010, 0x0010));
		auto PatientID = find(dcm,DcmTagKey(0x0010, 0x0020));
		auto StudyDate = find(dcm,DcmTagKey(0x0008, 0x0020));
		auto StudyTime = find(dcm,DcmTagKey(0x0008, 0x0030));
		auto SequenceNumber = find(dcm,DcmTagKey(0x0020, 0x0011));
		auto SequenceDescription = find(dcm,DcmTagKey(0x0008, 0x103e));
		auto InstanceUid = find(dcm,DcmTagKey(0x0008, 0x0018));
		return sroot /
			(PatientID.empty()?PatientName:PatientID) /
			(StudyDate.substr(2) + "_" + StudyTime.substr(0,6))/
			(std::string("S")+SequenceNumber+"_"+SequenceDescription)/
			InstanceUid;
	}
	return {};
}

bool makeDirectory(const fs::path &path){
	boost::system::error_code ec;
	if(!fs::create_directories(path,ec)){
		OrthancPlugins::LogError(std::string("Failed to create directory \"") + path.native() + "\" " + ec.message());
		return false;
	} else
		return true;
}
OrthancPluginErrorCode write(const char *uuid, const void *content, int64_t size, OrthancPluginContentType type){
	std::future<fs::path> shadow=std::async(GetSPath,content,size);
	fs::path org=GetOPath(uuid);

	//writing the original
	if(!makeDirectory(org.parent_path())){
		return OrthancPluginErrorCode_CannotWriteFile; //cannot write file
	}

	auto FILE = open(org.c_str(), O_CREAT|O_WRONLY);
	auto written=write(FILE, content, size);
	close(FILE);
	if(written<size) {
		OrthancPlugins::LogError(std::string("Failed to write \"") + org.native() + "\" " + strerror(errno));
		return OrthancPluginErrorCode_CannotWriteFile; //cannot write file
	}

	//creating the link
	fs::path spath = shadow.get();
//	if(makeDirectory(spath.parent_path())){
		int erg = link(org.c_str(), spath.c_str());
		if (erg == EXDEV) { //not same device, fallback to symlink
			erg = symlink(org.c_str(), spath.c_str());
		}
		if (erg) {
			OrthancPlugins::LogWarning(
				std::string(
					"Failed to write shadow \"") + spath.native() + "\" for \"" + uuid + "\" " +
					strerror(errno)
			);
		}
//	}
	return OrthancPluginErrorCode_Success;
}
OrthancPluginErrorCode read(void **content, int64_t *size, const char *uuid, OrthancPluginContentType type){
	fs::path org=GetOPath(uuid);
	boost::system::error_code ec;
	*size=fs::file_size(org,ec);
	if(ec){
		OrthancPlugins::LogError(
			std::string(
				"Failed to read \"") + org.native() + "\" for \"" + uuid + "\" " +
				ec.message()
		);
		return OrthancPluginErrorCode_InexistentFile; //cannot read file
	}
	auto FILE = open(org.c_str(), O_RDONLY);
	*content=malloc(*size);
	auto red=read(FILE, *content, *size);
	close(FILE);
	if(red!=0) {
		OrthancPlugins::LogError(std::string("Failed to read all of \"") + org.native() + "\" " + strerror(errno));
		return OrthancPluginErrorCode_CorruptedFile; //cannot read file
	} else
		return OrthancPluginErrorCode_Success;
}

void removeDir(fs::path dir){
	boost::system::error_code ec;
	while(fs::is_empty(dir)) {
		fs::remove(dir, ec);
		OrthancPlugins::LogError(
			std::string("Failed to remove directory \"") + dir.native() + "\" " + ec.message());
		dir=dir.parent_path();
		if(fs::equivalent(dir,oroot))
			break;
	}
}
OrthancPluginErrorCode remove(const char *uuid, OrthancPluginContentType type){
	void *content;
	int64_t size;
	std::future<OrthancPluginErrorCode> shadow;
	if(read(&content,&size,uuid,OrthancPluginContentType_Unknown)){
		shadow = std::async([content,size,&uuid]()->OrthancPluginErrorCode{
			auto path=GetSPath(content,size);
			if(unlink(path.c_str())){
				OrthancPlugins::LogError(
					std::string(
						"Failed to delete \"") + path.native() + "\" for \"" + uuid + "\" " +
						strerror(errno)
				);
				return OrthancPluginErrorCode_CorruptedFile;
			} else {
				removeDir(path.parent_path());
				return OrthancPluginErrorCode_Success;
			}
		});
	}

	fs::path org=GetOPath(uuid);
	if(unlink(org.c_str())){
		OrthancPlugins::LogError(
			std::string("Failed to delete \"") +
			org.native() + "\" for \"" + uuid + "\" " +
			strerror(errno)
		);
		return OrthancPluginErrorCode_CorruptedFile;
	}
	return shadow.get();
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

	sroot=OrthancPlugins::OrthancConfiguration().GetStringValue("ShadowPath","");
	oroot=OrthancPlugins::OrthancConfiguration().GetStringValue("StorageDirectory","");

	if(OrthancPlugins::OrthancConfiguration().GetBooleanValue("StorageCompression",false)){
		OrthancPlugins::LogError("\"StorageCompression\" is switched on. Using the shadow writer plugin would be pointless");
		return -1;
	}

	if(oroot.empty()){
		OrthancPlugins::LogError("Loaded shadow writer plugin. But \"StorageDirectory\" in the configuration is not set");
		return -1;
	}

	if(sroot.empty()){
		OrthancPlugins::LogError("Loaded shadow writer plugin. But \"ShadowPath\" in the configuration is not set");
		return -1;
	}

	OrthancPluginRegisterStorageArea(c,write,read,remove);
	OrthancPlugins::LogInfo(std::string("Loaded shadow writer plugin. Shadow root is ")+sroot.native());
	return 0;
}


ORTHANC_PLUGINS_API void OrthancPluginFinalize()
{
}


ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
{
	return "shadow writer";
}


ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
{
	return "0.0";
}
}
