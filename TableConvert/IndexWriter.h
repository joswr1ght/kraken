#ifndef INDEXWRITER_H
#define INDEXWRITER_H

#include "BaseWriter.h"
#include <string>
#include <stdio.h>

class IndexWriter : public BaseWriter {
public:
    IndexWriter(std::string &path, std::string &index, unsigned int blockSize);
    virtual ~IndexWriter();
    virtual void Write(uint64_t endpoint, uint64_t index);
    virtual bool isOK() {return mIsOK;}

private:
    FILE* mFile;
    FILE* mIndexFile;
    unsigned int   mBlockSize;
    unsigned char* mBuffer;
    unsigned int mBufPos;
    unsigned int mBitBuffer;
    unsigned int mBitCount;
    bool mIsOK;
    uint64_t mLast;
    unsigned int mRadixCount;
    uint64_t mBitsWritten;
    uint64_t mChainsWritten;
    uint64_t mLastEnd;
    /* Compact size for cache coherency */
    unsigned short mBase[4096];
    unsigned char mBits[4096];
    unsigned char mGroup[4096];
};

#endif
