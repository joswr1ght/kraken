#define _FILE_OFFSET_BITS 64

#include "DeltaWriter.h"
#include <sys/stat.h>
#include <assert.h>

#define BUFSIZE (11*4096)

/* This write macro only works as intended if a minimum of 8 bits are beeing written */
#define WRITE()\
    mBitsWritten+=bout; \
    if (mBitCount) { \
        unsigned int b = (unsigned int)(data>>(mBitCount+bout-8)); \
        mBuffer[mBufPos++]=(unsigned char)b|mBitBuffer; \
        bout-=(8-mBitCount); \
    } \
    while(bout>=8) { \
        mBuffer[mBufPos++] = (unsigned char)(data>>(bout-8));  \
        bout-=8; \
    } \
    mBitBuffer = (data<<(8-bout))&0x0ff;        \
    mBitCount = bout;

using namespace std;

DeltaWriter::DeltaWriter(std::string &path) :
    mBufPos(0),
    mBitBuffer(0),
    mBitCount(0),
    mIsOK(true),
    mLast(0ULL),
    mBitsWritten(0),
    mChainsWritten(0)
{
    mFile = fopen(path.c_str(),"w");
    mIsOK = (mFile!=NULL);

    /* Fill in encoding tables */
    int groups[] = {0,4,126,62,32,16,8,4,2,1};
    int gsize = 1;
    unsigned short base = 0;
    int group = 0;
    for (int i=0;i<10;i++) {
        for (int j=0; j<groups[i]; j++) {
            for(int k=0; k<gsize; k++) {
                mBase[base+k] = base;
                mBits[base+k] = i;
                mGroup[base+k] = (unsigned char)group;
            }
            base += gsize;
            group++;
        }
        gsize = 2 * gsize;
    }
    /* The rest should be unused */
    assert(base<4096);
    assert(group<256);
    mRadixCount = base;
}

DeltaWriter::~DeltaWriter()
{
    if (mFile) {
        if (mBufPos) {
            fwrite(mBuffer,1,mBufPos,mFile);
        }
        if (mBitCount) {
            unsigned char ch = (unsigned char)mBitBuffer;
            fwrite(&ch,1,1,mFile);
        }
        fclose(mFile);
    }
    printf("%lli chains written, %f bits pr chain.\n",
           mChainsWritten,
           (float)mBitsWritten/(float)mChainsWritten);
}

void DeltaWriter::Write(uint64_t endpoint, uint64_t index)
{
    uint64_t delta = endpoint - mLast;
    mLast = endpoint;

    delta = delta >> 12;
    uint64_t radix = delta>>10;
    uint64_t data;
    int bout;

    if ((radix>=mRadixCount)||(index>=0x200000000ULL)) {
        /* Write escape + raw */
        assert(delta<0x40000000ULL);
        assert(index<0x400000000ULL);
        /* Write oxff */
        bout = 8;
        data = 0xff;
        WRITE();
        bout = 64;
        data = (delta<<34)|index;
    } else {
        unsigned int r = (unsigned int)radix;
        int group = mGroup[r];
        int off = r - mBase[r];
        int bits = mBits[r];
        /* data to be written */
        bout = 33+18+bits;
        data = (group<<(10+bits))|(off<<10)|(delta&0x03ffULL);
        data = (data<<33)|index;
    }
    WRITE();
    mChainsWritten++;
    if (mBufPos>1010) {
        assert(mFile);
        fwrite(mBuffer,1,mBufPos,mFile);
        mBufPos=0;
    }
}
