//
// Created by enrico on 18.03.21.
//

#include "dicomhandle.hpp"
DicomHandle::DicomHandle(const void *buffer, size_t size)
{
	DcmInputBufferStream is;
	if (size > 0) {
		is.setBuffer(buffer, size);
	}
	is.setEos();

	transferInit();

	if (read(is, EXS_Unknown, EGL_noChange, size).good())
	{
		valid=true;
		loadAllDataIntoMemory();
		transferEnd();
	}
}
std::string DicomHandle::findString(const DcmTagKey &key)
{
	DcmStack resultStack;
	search(key, resultStack);
	if(resultStack.empty()){
		OrthancPlugins::LogError(std::string("Failed to findString dicom tag \"") + DcmTag(key).getTagName());
		return {};
	}
	auto element=dynamic_cast<DcmElement*>(resultStack.top());
	if(element == nullptr){
		OrthancPlugins::LogError(std::string("Found tag \"") + DcmTag(key).getTagName() + " is not a DcmElement");
		return {};
	}
	OFString ret;
	element->getOFString(ret,0);
	return ret.c_str();
}
bool DicomHandle::replaceString(DcmTagKey key, std::string replacement)
{
	DcmDataset *dset = getDataset();
	if(dset->putAndInsertOFStringArray(key,replacement.c_str()).good()){
		OrthancPlugins::LogError(std::string("Failed to replace ") + DcmTag(key).getTagName() +" with \""+replacement+"\"");
		return false;
	} else
		return true;
}
bool DicomHandle::SaveToMemoryBuffer(OrthancPluginMemoryBuffer *target)
{
	DcmDataset *dataSet = getDataset();

	E_TransferSyntax xfer = dataSet->getOriginalXfer();
	if (xfer == EXS_Unknown)
	{
		// No information about the original transfer syntax: This is
		// most probably a DICOM dataset that was read from memory.
		xfer = EXS_LittleEndianExplicit;
	}

	E_EncodingType encodingType = /*opt_sequenceType*/ EET_ExplicitLength;

//		// Create the meta-header information
	//do we need this - should already be in place, right ?
//		validateMetaInfo(xfer);
//		removeInvalidGroups();

	// Create a memory buffer with the proper size
	const uint32_t estimatedSize = calcElementLength(xfer, encodingType);  // (*)
	OrthancPluginCreateMemoryBuffer(OrthancPlugins::GetGlobalContext(), target, estimatedSize);

	DcmOutputBufferStream ob(target->data, estimatedSize);

	// Fill the memory buffer with the meta-header and the dataset
	transferInit();
	OFCondition c = write(ob, xfer, encodingType, nullptr,
		/*opt_groupLength*/ EGL_recalcGL,
		/*opt_paddingType*/ EPD_withoutPadding);
	transferEnd();

	if (c.good()) {
		ob.flush();
		return true;
	} else {
		// Error
		OrthancPluginFreeMemoryBuffer(OrthancPlugins::GetGlobalContext(), target);
		return false;
	}
}
