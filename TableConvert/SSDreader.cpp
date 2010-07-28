#define _FILE_OFFSET_BITS 64

#include "SSDreader.h"
#include <sys/stat.h>
#include <assert.h>
#include <string.h>

SSDreader::SSDreader(std::string &path, std::string indexpath) :
    mBufPos(0),
    mIsOK(true),
    mNumBlocks(0)
{
    mFile = fopen(path.c_str(),"r");
    if (!mFile) {
        fprintf(stderr,"ERROR: Could not open file %s for reading.\n",
                path.c_str());
        mIndexFile = NULL;
        mIsOK = false;
        return;
    }
    mIndexFile = fopen(indexpath.c_str(),"r");
    if (!mIndexFile) {
        fprintf(stderr,"ERROR: Could not open file %s for reading.\n",
                indexpath.c_str());
        fclose(mFile);
        mFile = NULL;
        mIsOK = false;
        return;
    }
    fseek(mIndexFile ,0 ,SEEK_END );
    size_t index_size = ftell(mIndexFile);
    fseek(mIndexFile ,0 ,SEEK_SET );
    mNumBlocks = index_size / sizeof(uint64_t);

    if (!mNumBlocks) {
        mIsOK = false;
        fclose(mFile);
        mFile = NULL;
        fclose(mIndexFile);
        mIndexFile=NULL;
    }
    size_t r = fread(mBuffer,sizeof(uint64_t),512,mFile);
    assert(r==512);
    r = fread(&mFirst,sizeof(uint64_t),1,mIndexFile);
    mNumBlocks--;
}

SSDreader::~SSDreader()
{
    if (mFile) fclose(mFile);
    if (mIndexFile) fclose(mIndexFile);
}

bool SSDreader::Read(uint64_t& endpoint, uint64_t& index)
{
    if (mBufPos>=512) {
        assert(mNumBlocks); /* index truncated ? */
        size_t r = fread(mBuffer,sizeof(uint64_t),512,mFile);
        assert(r==512);
        r = fread(&mFirst,sizeof(uint64_t),1,mIndexFile);
        mNumBlocks--;
        mBufPos=0;
    }

    uint64_t data = mBuffer[mBufPos++];
    index = data >> 30;
    endpoint = mFirst+((data&0x3FFFFFFFULL)<<12);
    if (mNumBlocks==0) {
        if (mBufPos>=512) return false;
        if (mBuffer[mBufPos]==0xffffffffffffffffULL) return false;
    }
    return true;
}
