//
// Created by enrico on 19.03.21.
//

#include "tagprocessorlist.hpp"

std::string TagProcessingGenerator::generate(DcmDataset &dset) const
{
	OFString original;
	if(!dset.findAndGetOFString(tag,original).good())
		return "";
	if(regex) {
		std::cmatch what;
		if (!std::regex_match(original.begin(), original.end(), what, *regex))
			return "";
		return what.format(replacement);
	} else
		return replacement;
}

TagProcessorList::TagProcessorList(const OrthancPlugins::OrthancConfiguration &configuration)
{
	const DcmDataDictionary &dict=GlobalDcmDataDictionary().rdlock();
	const Json::Value &cfg=configuration.GetJson();
	clear();

	for(const auto &tag_name:cfg.getMemberNames()) {
		const DcmDictEntry *entry = dict.findEntry(tag_name.c_str());
		if (entry == nullptr) {
			OrthancPlugins::LogError(
				std::string("Dicom tag ") + tag_name + " not found. Skipping entry for processing.");
			continue;
		}

		if (cfg[tag_name].isMember("delete") && cfg[tag_name]["delete"].asBool()) { // simple deletion
			push_back({entry->getKey()});
		}
		else {
			TagProcessor processor{entry->getKey()};
			if (!cfg[tag_name].isMember("replace")) {
				OrthancPlugins::LogError(std::string("Processing for dicom tag ") + tag_name
											 + R"( does not have a "replace" entry and is not "delete". Skipping ...)");
				continue;
			}
			else {
				if (!cfg[tag_name]["replace"].isArray()) {
					OrthancPlugins::LogError(
						std::string("\"replace\" for ") + tag_name + " is no an array, Skipping ...");
					continue;
				}
				bool bad = false;
				for (Json::Value replacer:cfg[tag_name]["replace"]) {
					TagProcessingGenerator generator;
					if (replacer.isArray()) {
						Json::Value buffer;
						switch (replacer.size()) {
							case 3: {
								replacer.removeIndex(0, &buffer);
								entry = dict.findEntry(buffer.asCString());
								if (entry == nullptr) {
									OrthancPlugins::LogError(std::string("Dicom tag ") + replacer[0].asString()
																 + " not found. Skipping entry for processing.");
									bad = true;
								}
								else {
									generator.tag = entry->getKey();
								}
							}
							case 2: {
								replacer.removeIndex(0, &buffer);
								generator.regex = std::regex(buffer.asString(),
															 std::regex_constants::ECMAScript
																 | std::regex_constants::optimize);
							}
							case 1: {
								replacer.removeIndex(0, &buffer);
								generator.replacement = buffer.asString();
							}
								break;
							default:
								OrthancPlugins::LogError(
									"The inner replacement array cannot be short than 1 or longer than 3");
								bad = true;
						}
					}
					else {
						generator.replacement = replacer.asString();
					}
					processor.generators.push_back(std::move(generator));
				}
				if (bad) // if replacers where bad, skip the whole processor
					continue;
			}
			if (cfg[tag_name].isMember("mask")) {
				processor.mask = std::regex(cfg[tag_name]["mask"].asString(),
											std::regex_constants::ECMAScript | std::regex_constants::optimize);
			}
			push_back(std::move(processor));
		}
	}
}
