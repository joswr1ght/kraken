#ifndef SSD_READER_H
#define SSD_READER_H

#include "BaseReader.h"
#include <string>
#include <stdio.h>

class SSDreader : public BaseReader {
public:
    SSDreader(std::string &path, std::string indexpath);
    virtual ~SSDreader();
    virtual bool Read(uint64_t& endpoint, uint64_t& index);
    virtual bool isOK() {return mIsOK;}

private:
    FILE* mFile;
    FILE* mIndexFile;
    uint64_t mBuffer[512];
    unsigned int mBufPos;
    bool mIsOK;
    uint64_t mFirst;
    unsigned int mNumBlocks;  /* number of block left */
};

#endif
