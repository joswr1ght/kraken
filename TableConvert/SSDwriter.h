#ifndef SSD_WRITER_H
#define SSD_WRITER_H

#include "BaseWriter.h"
#include <string>
#include <stdio.h>

class SSDwriter : public BaseWriter {
public:
    SSDwriter(std::string &path, std::string indexpath);
    virtual ~SSDwriter();
    virtual void Write(uint64_t endpoint, uint64_t index);
    virtual bool isOK() {return mIsOK;}

private:
    void Flush();

    FILE* mFile;
    FILE* mIndexFile;
    uint64_t mBuffer[512];
    unsigned int mBufPos;
    bool mIsOK;
    uint64_t mFirst;
};

#endif
