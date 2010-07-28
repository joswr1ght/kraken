#ifndef MULTI_FILE_READER_H
#define MULTI_FILE_READER_H

#include "BaseReader.h"
#include <string>
#include <stdio.h>

class MultiFileReader : public BaseReader {
public:
    MultiFileReader(std::string &path);
    virtual ~MultiFileReader();
    virtual bool Read(uint64_t& endpoint, uint64_t& index);
    virtual bool isOK() {return mIsOK;}

private:
    bool NextFile();
    bool ReadFile();

    FILE* mFile;
    unsigned char mBuffer[4096*11];
    unsigned int mBufPos;
    bool mIsOK;
    uint64_t mFirst;
    unsigned int mNumFile;
    unsigned int mRemain;  /* number of bytes left in current file */
    std::string mBaseName;
    unsigned int mLast;
};

#endif
