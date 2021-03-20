//
// Created by enrico on 18.03.21.
//

#ifndef DICOMHANDLE_HPP
#define DICOMHANDLE_HPP

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcostrmb.h>
#include "OrthancPluginCppWrapper.h"

class DicomHandle:public DcmFileFormat
{
public:
	bool valid=false;
	DicomHandle(const void* buffer, size_t size);
	bool SaveToMemoryBuffer(OrthancPluginMemoryBuffer *target);
	std::string findString(const DcmTagKey& key);
	bool replaceString(DcmTagKey key, std::string replacement);
};


#endif //DICOMHANDLE_HPP
