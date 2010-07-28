#ifndef MULTIFILEWRITER_H
#define MULTIFILEWRITER_H

#include "BaseWriter.h"
#include <stdint.h>
#include <string>
#include <stdio.h>

class MultiFileWriter : public BaseWriter {
public:
    MultiFileWriter(std::string &path);
    virtual ~MultiFileWriter();
    void Write(uint64_t endpoint, uint64_t index);
    bool isOK() {return mIsOK;}

private:
    FILE* mFiles[256];
    unsigned char* mBuffers[256];
    unsigned int mBufSize[256];
    bool mIsOK;
};

#endif
