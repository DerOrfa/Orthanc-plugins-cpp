//
// Created by enrico on 18.03.21.
//

#ifndef PATIENTNAMEMAPPING_HPP
#define PATIENTNAMEMAPPING_HPP

#include "OrthancPluginCppWrapper.h"
#include <boost/filesystem.hpp>

using boost::filesystem::path;

class PatientNameMapping {
	path filename;
	size_t relevant_chars=0;
	char terminator=0;
	bool reverse=false;
	std::chrono::time_point<std::chrono::system_clock> last_update;
	std::map<std::string, std::string> map;
	std::set<std::string> values;

	void update();

public:
	PatientNameMapping(const OrthancPlugins::OrthancConfiguration &map);

	std::string lookup(std::string org);
	bool knownValue(std::string value);
};

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
void RegisterPatientNameMapping(const OrthancPlugins::OrthancConfiguration &map);

#endif //PATIENTNAMEMAPPING_HPP
