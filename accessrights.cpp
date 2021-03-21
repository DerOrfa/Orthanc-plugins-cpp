//
// Created by enrico on 21.03.21.
//
#include "OrthancPluginCppWrapper.h"
#include <regex>

std::regex localIpRegex_;

int32_t http_request_filter(OrthancPluginHttpMethod method, const char *uri, const char *ip, uint32_t headersCount, const char *const *headersKeys, const char *const *headersValues){

	if(!std::regex_match(ip,localIpRegex_)){
		return 0; //reject with 403
	}

//	if(method==OrthancPluginHttpMethod_Get) //all "local" GETs are ok
//		return 1;

	return 1;
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

	localIpRegex_ = std::regex(
	OrthancPlugins::OrthancConfiguration().GetStringValue("LocalIpRegex", "127.0.0.1"),
		std::regex_constants::ECMAScript | std::regex_constants::optimize
	);
	OrthancPluginRegisterIncomingHttpRequestFilter 	(c,http_request_filter);

	return 0;
}

ORTHANC_PLUGINS_API void OrthancPluginFinalize(){}
ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
{
	return "http access filter";
}
ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion(){return "0.0";}
}
