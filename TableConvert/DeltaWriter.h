#ifndef DELTAWRITER_H
#define DELTAWRITER_H

#include "BaseWriter.h"
#include <string>
#include <stdio.h>

class DeltaWriter : public BaseWriter {
public:
    DeltaWriter(std::string &path);
    virtual ~DeltaWriter();
    virtual void Write(uint64_t endpoint, uint64_t index);
    virtual bool isOK() {return mIsOK;}

private:
    FILE* mFile;
    unsigned char mBuffer[1024];
    unsigned int mBufPos;
    unsigned int mBitBuffer;
    unsigned int mBitCount;
    bool mIsOK;
    uint64_t mLast;
    unsigned int mRadixCount;
    uint64_t mBitsWritten;
    uint64_t mChainsWritten;
    /* Compact size for cache coherency */
    unsigned short mBase[4096];
    unsigned char mBits[4096];
    unsigned char mGroup[4096];
};

#endif
