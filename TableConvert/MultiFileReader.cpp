#define _FILE_OFFSET_BITS 64

#include "MultiFileReader.h"
#include <sys/stat.h>
#include <assert.h>
#include <string.h>

#define BUFSIZE (11*4096) 

static inline uint64_t getEnd(unsigned char* p)
{
    uint64_t e = 0;

    e =    (uint64_t)p[0]      | ((uint64_t)p[1]<<8)
        | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24)
        | ((uint64_t)p[4]<<32) | ((uint64_t)p[5]<<40);

    return e;
}

static inline uint64_t getIndex(unsigned char* p)
{
    uint64_t e = 0;

    e =    (uint64_t)p[6]      | ((uint64_t)p[7]<<8)
        | ((uint64_t)p[8]<<16) | ((uint64_t)p[9]<<24)
        | ((uint64_t)p[10]<<32);

    return e;
}

MultiFileReader::MultiFileReader(std::string &path) :
    mFile(NULL),
    mIsOK(true),
    mNumFile(0),
    mBaseName(path)
{
    mIsOK = NextFile();
}

MultiFileReader::~MultiFileReader()
{
    if (mFile) fclose(mFile);
}

bool MultiFileReader::NextFile()
{
    if (mNumFile>=256) return false;

    if (mFile) fclose(mFile);
    char name[16];
    snprintf(name,16,"/%02x.tbl",mNumFile);
    std::string filename = mBaseName+std::string(name);
    mFile = fopen(filename.c_str(),"r");
    if (mFile==NULL) {
        fprintf(stderr,"ERROR: Could not open file %s for reading.\n",
                filename.c_str());
        return false;
    }
    mFirst = ((uint64_t)mNumFile)<<56;

    fseek(mFile ,0 ,SEEK_END );
    mRemain = ftell(mFile);
    fseek(mFile ,0 ,SEEK_SET );

    mNumFile++;
    return ReadFile();
}

bool MultiFileReader::ReadFile()
{
   size_t num;
   if (mRemain < BUFSIZE) {
       num = mRemain;
   } else {
       num = BUFSIZE;
   }
   mLast = num;
   mRemain-=num;
   if (num==0) {
       return NextFile();
   } else {
       size_t r = fread(mBuffer,1,num,mFile);
       assert(r==num);
       mBufPos = 0;
   }
   return true;
}

bool MultiFileReader::Read(uint64_t& endpoint, uint64_t& index)
{
    endpoint = mFirst|(getEnd(&mBuffer[mBufPos])<<8);
    index = getIndex(&mBuffer[mBufPos]);
    
    mBufPos+=11;

    if (mBufPos>=mLast) {
        return ReadFile();
    }

    return true;
}
