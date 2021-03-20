//
// Created by enrico on 19.03.21.
//

#ifndef TAGPROCESSORLIST_HPP
#define TAGPROCESSORLIST_HPP

#include "OrthancPluginCppWrapper.h"

#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <json/json.h>
#include <regex>
#include <list>

struct TagProcessingGenerator {
	DcmTagKey tag;
	boost::optional<std::regex> regex;
	std::string replacement;
	std::string generate(DcmDataset& dset)const;
};
struct TagProcessor{
	DcmTagKey tag;
	boost::optional<std::regex> mask;
	std::list<TagProcessingGenerator> generators;
};

class TagProcessorList: public std::list<TagProcessor>
{
public:
	TagProcessorList(const OrthancPlugins::OrthancConfiguration &configuration);
	void FixTags(DcmDataset *dset)
	{
		for(const auto &proc : *this){
			OFString original;
			auto erg=dset->findAndGetOFString(proc.tag,original);
			if(proc.mask && !std::regex_match(original.begin(), original.end(), *proc.mask)){
				continue;//existing mask did not match, don't process this Tag
			}

			if(proc.generators.empty()){ // is a simple delete
				dset->findAndDeleteElement(proc.tag,true,true);
			} else {
				std::string replacement;
				for (const auto &generator:proc.generators) {
					replacement+=generator.generate(*dset);
				}
				if(!dset->putAndInsertOFStringArray(proc.tag,replacement.c_str()).good())
					OrthancPlugins::LogError(std::string("Failed set ") + DcmTag(proc.tag).getTagName() + " to " + replacement);
			}
		}
	}

};


#endif //TAGPROCESSORLIST_HPP
