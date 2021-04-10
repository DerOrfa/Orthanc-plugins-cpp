//
// Created by enrico on 18.03.21.
//

#include "patientnamemapping.hpp"

std::string PatientNameMapping::lookup(std::string org)
{
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

bool PatientNameMapping::knownValue(std::string value)
{
	return values.find(value)!=values.end();
}

PatientNameMapping::PatientNameMapping(const OrthancPlugins::OrthancConfiguration &map)
{
	filename=map.GetStringValue("File","");
	terminator=map.GetStringValue("Terminator","|")[0];
	relevant_chars = map.GetIntegerValue("RelevantChars",4);
	reverse=map.GetBooleanValue("Reverse",false);

	if(exists(filename))
		update();
	else
		OrthancPlugins::LogError(std::string("Patient name map file \"") + filename.native() + "\" does not exist");
}

void PatientNameMapping::update()
{
	std::ifstream in(filename.c_str());
	in.exceptions(std::ifstream::badbit);
	map.clear();
	values.clear();
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
