#define _FILE_OFFSET_BITS 64

#include "DeltaReader.h"
#include <sys/stat.h>
#include <assert.h>
#include <string.h>

using namespace std;

#define READ8()\
    bits = (mBitBuffer>>(mBitCount-8))&0xff;                 \
    mBitBuffer = (mBitBuffer<<8)|mBuffer[mBufPos++];

#define READN(n)\
    bits = mBitBuffer>>(mBitCount-(n));         \
    bits = bits & ((1<<(n))-1);                 \
    mBitCount-=(n);                             \
    if(mBitCount<8) { \
        mBitBuffer = (mBitBuffer<<8)|mBuffer[mBufPos++];    \
        mBitCount+=8; \
    } 

DeltaReader::DeltaReader(std::string &path) :
    mBufPos(0),
    mBitBuffer(0),
    mBitCount(0),
    mIsOK(true),
    mLast(0ULL),
    mLastBlock(0)
{
    mFile = fopen(path.c_str(),"r");
    mIsOK = (mFile!=NULL);

    if (!mIsOK) {
        fprintf(stderr,"ERROR: Could not open %s for reading.\n",path.c_str());
        return;
    }

    /* Fill in encoding tables */
    int groups[] = {0,4,126,62,32,16,8,4,2,1};
    int gsize = 1;
    unsigned short base = 0;
    int group = 0;
    for (int i=0;i<10;i++) {
        for (int j=0; j<groups[i]; j++) {
            mBase[group] = base;
            mBits[group] = i;
            base += gsize;
            group++;
        }
        gsize = 2 * gsize;
    }
    /* The rest should be unused */
    assert(group<256);

    /* read first block */
    size_t r = fread(mBuffer,1,1024,mFile);
    if (r!=1024) {
        mLastBlock = r;
    }
    if (r==0) {
        mIsOK = false;
    } else {
        mBitBuffer = mBuffer[mBufPos++];
        mBitCount = 8;
    }
}

DeltaReader::~DeltaReader()
{
    if (mFile) {
        fclose(mFile);
    }
}

bool DeltaReader::Read(uint64_t& endpoint, uint64_t& index)
{
    unsigned char bits;
    uint64_t delta;
    uint64_t tmp;

    READ8();
    if (bits==0xff) {
        /* Escape code */
        READ8();
        tmp = bits;
        READ8();
        tmp = (tmp<<8)|bits;
        READ8();
        tmp = (tmp<<8)|bits;
        READ8();
        tmp = (tmp<<8)|bits;
        READ8();
        tmp = (tmp<<8)|bits;
        READ8();
        tmp = (tmp<<8)|bits;
        READ8();
        tmp = (tmp<<8)|bits;
        READ8();
        tmp = (tmp<<8)|bits;
        delta = tmp >> 34;
        index = tmp & 0x3ffffffffULL;
    } else {
        unsigned int code = bits;
        unsigned int rb = mBits[code];
        // printf("%02x - %i - %x ",code,rb,mBase[code]);
        delta = mBase[code];
        unsigned int d2 = 0;
        if (rb>=8) {
            READ8();
            d2 = (d2<<8)|bits;
            rb-=8;
        }
        if (rb) {
            READN(rb);
            d2 = (d2<<rb)|bits;
        }
        // printf("%llx %x\n",delta,d2);
        delta+=d2;
        READ8();
        delta = (delta<<8)|bits;
        READN(2);
        delta = (delta<<2)|bits;

        READN(1);
        uint64_t idx = bits;
        READ8();
        idx = (idx<<8)|bits;
        READ8();
        idx = (idx<<8)|bits;
        READ8();
        idx = (idx<<8)|bits;
        READ8();
        index = (idx<<8)|bits;
    }

    /* Read more into buffer if needed */
    if (mBufPos>1014) {
        size_t rem = 1024-mBufPos; // remaining
        if (rem) {
            memcpy(mBuffer,&mBuffer[mBufPos],rem);
        }
        mBufPos = 0;
        size_t read = 1024 - rem;
        size_t r = fread(&mBuffer[rem],1,read,mFile);
        if (r<read) {
            mLastBlock = rem+r;
        }
    }


    mLast += (delta<<12);
    endpoint = mLast;

    return mLastBlock ? (mBufPos<mLastBlock) : true;
}

