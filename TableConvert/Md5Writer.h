#ifndef MD5_WRITER_H
#define MD5_WRITER_H

#include "BaseWriter.h"
#include <string>
#include <stdio.h>
#include "md5.h"

class Md5Writer : public BaseWriter {
public:
    Md5Writer();
    virtual ~Md5Writer();
    virtual void Write(uint64_t endpoint, uint64_t index);
    virtual bool isOK() {return mIsOK;}

private:
    bool mIsOK;
    class MD5* mHash;
};

#endif
