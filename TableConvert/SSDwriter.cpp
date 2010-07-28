#define _FILE_OFFSET_BITS 64

#include "SSDwriter.h"
#include <sys/stat.h>
#include <assert.h>
#include <string.h>
#include <iostream>

SSDwriter::SSDwriter(std::string &path, std::string indexpath) :
    mBufPos(0),
    mIsOK(true)
{
    mFile = fopen(path.c_str(),"r");
    if (mFile) {
        fprintf(stderr,"ERROR: %s exists, will not overwrite.\n",
                path.c_str());
        fclose(mFile);
        mFile = NULL;
        mIndexFile = NULL;
        mIsOK = false;
        return;
    }
    mFile = fopen(path.c_str(),"w");
    if (!mFile) {
        fprintf(stderr,"ERROR: Could not open file %s for writing.\n",
                path.c_str());
        mIndexFile = NULL;
        mIsOK = false;
        return;
    }
    mIndexFile = fopen(indexpath.c_str(),"r");
    if (mIndexFile) {
        fprintf(stderr,"ERROR: Could not open file %s for reading.\n",
                indexpath.c_str());
        fclose(mIndexFile);
        fclose(mFile);
        mIndexFile = NULL;
        mFile = NULL;
        mIsOK = false;
        return;
    }
    mIndexFile = fopen(indexpath.c_str(),"w");
    if (!mIndexFile) {
        fprintf(stderr,"ERROR: Could not open file %s for writing.\n",
                indexpath.c_str());
        fclose(mFile);
        mFile = NULL;
        mIsOK = false;
        return;
    }
    /* max value padding for last block */
    memset(mBuffer,255,4096);
}

SSDwriter::~SSDwriter()
{
    if (mBufPos) Flush();
    if (mFile) fclose(mFile);
    if (mIndexFile) fclose(mIndexFile);
}

void SSDwriter::Flush()
{
    size_t r = fwrite(&mFirst,sizeof(uint64_t),1,mIndexFile);
    assert(r==1);
    r = fwrite(mBuffer,sizeof(uint64_t),512,mFile);
    assert(r==512);
    /* max value padding for last block */
    memset(mBuffer,255,4096);
    mBufPos = 0;
}

void SSDwriter::Write(uint64_t endpoint, uint64_t index)
{
    if (mBufPos==0) {
        mFirst = endpoint;
    }
    uint64_t r = (endpoint - mFirst)>>12;
    if (r>=0x40000000) {
        std::cout << "End point error: " << std::hex << r
                  << " " << mFirst << " " << endpoint << "\n";
        assert(!"End points to sparse. Table may be too small.");
    }
    r = r | (index<<30);
    mBuffer[mBufPos++]=r;
    if (mBufPos>=512) Flush();
}
