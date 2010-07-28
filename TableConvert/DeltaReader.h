#ifndef DELTAREADER_H
#define DELTAREADER_H

#include "BaseReader.h"
#include <string>
#include <stdio.h>

class DeltaReader : public BaseReader {
public:
    DeltaReader(std::string &path);
    virtual ~DeltaReader();
    virtual bool Read(uint64_t& endpoint, uint64_t& index);
    virtual bool isOK() {return mIsOK;}

private:
    FILE* mFile;
    unsigned char mBuffer[1024];
    unsigned int mBufPos;
    unsigned int mBitBuffer;
    unsigned int mBitCount;
    bool mIsOK;
    uint64_t mLast;
    unsigned int mLastBlock;
    /* Compact size for cache coherency */
    unsigned short mBase[256];
    unsigned char mBits[256];
};

#endif
