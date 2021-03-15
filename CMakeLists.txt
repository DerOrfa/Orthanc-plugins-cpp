cmake_minimum_required(VERSION 2.8)
project(orthanc_shadowwriter)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED yes)

find_package(DCMTK REQUIRED)
find_package(OpenSSL REQUIRED)

SET(USE_SYSTEM_JSONCPP ON CACHE BOOL "Use the system version of JsonCpp")
SET(USE_SYSTEM_BOOST ON CACHE BOOL "Use the system version of boost")

SET(ORTHANC_PLUGINS_COMMON_PATH CACHE PATH "Path to the \"common\" directory in the \"plugins/sample\" directory of orthanc")
SET(ORTHANC_FRAMEWORK_PATH CACHE PATH "Path to the orthanc framework directory")

find_file(ORTHANC_PLUGINS_CMAKE "OrthancPlugins.cmake" REQUIRED)


include(${ORTHANC_PLUGINS_COMMON_PATH}/OrthancPlugins.cmake)
include(${ORTHANC_FRAMEWORK_PATH}/Resources/CMake/JsonCppConfiguration.cmake)
include(${ORTHANC_FRAMEWORK_PATH}/Resources/CMake/BoostConfiguration.cmake)

add_definitions(-DHAS_ORTHANC_EXCEPTION=0)

include_directories(${ORTHANC_PLUGINS_COMMON_PATH})

message("Setting the version of the plugin to ${SERVE_FOLDERS_VERSION}")
add_definitions(-DSERVE_FOLDERS_VERSION="${SERVE_FOLDERS_VERSION}")

add_library(orthanc_shadowwriter SHARED
	shadowwriter.cpp
	${ORTHANC_PLUGINS_COMMON_PATH}/OrthancPluginCppWrapper.cpp
	${JSONCPP_SOURCES} ${BOOST_SOURCES}
)
target_link_libraries(orthanc_shadowwriter ${DCMTK_LIBRARIES})
set_target_properties(orthanc_shadowwriter PROPERTIES VERSION ${SERVE_FOLDERS_VERSION} SOVERSION ${SERVE_FOLDERS_VERSION})

add_library(orthanc_instancefilter SHARED
	instancefilter.cpp
	${ORTHANC_PLUGINS_COMMON_PATH}/OrthancPluginCppWrapper.cpp
	${JSONCPP_SOURCES}
)
target_link_libraries(orthanc_instancefilter ${DCMTK_LIBRARIES} OpenSSL::SSL)
set_target_properties(orthanc_instancefilter PROPERTIES VERSION ${SERVE_FOLDERS_VERSION} SOVERSION ${SERVE_FOLDERS_VERSION})

#add_library(orthanc_dicomprocessor SHARED
#	dicomtransform.cpp
#	${ORTHANC_PLUGINS_COMMON_PATH}/OrthancPluginCppWrapper.cpp
#	${JSONCPP_SOURCES}
#)
#target_link_libraries(orthanc_dicomprocessor ${DCMTK_LIBRARIES} )
#set_target_properties(orthanc_dicomprocessor PROPERTIES VERSION ${SERVE_FOLDERS_VERSION} SOVERSION ${SERVE_FOLDERS_VERSION})
