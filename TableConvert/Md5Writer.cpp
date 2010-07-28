#include "Md5Writer.h"
#include "md5.h"
#include <assert.h>
#include <string.h>
#include <iostream>

Md5Writer::Md5Writer()
{
    mHash = new MD5(); 
    mIsOK = (mHash!=NULL);
}

Md5Writer::~Md5Writer()
{
    if (mHash) {
        mHash->finalize();
        char* digest = mHash->hex_digest();
        printf("md5hash: %s\n",digest);
        delete[] digest;
        delete mHash;
    }
}


void Md5Writer::Write(uint64_t endpoint, uint64_t index)
{
    unsigned char chunk[16];
    unsigned char* ch = chunk;
    // Force little endian
    for(int i=0;i<8;i++) {
        *ch++ = (unsigned char)endpoint;
        endpoint = endpoint >> 8;
    }
    for(int i=0;i<8;i++) {
        *ch++ = (unsigned char)index;
        index = index >> 8;
    }
    mHash->update(chunk,16);
}
