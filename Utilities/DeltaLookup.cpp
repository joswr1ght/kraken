#define _FILE_OFFSET_BITS 64

#include "DeltaLookup.h"
#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <list>
#include <sys/time.h>
#include <assert.h>
#include <linux/fs.h>
#include <stdlib.h>
#include <stropts.h>
#include <fcntl.h>

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


bool DeltaLookup::mInitStatics = false;
unsigned short DeltaLookup::mBase[256];
unsigned char DeltaLookup::mBits[256];

DeltaLookup::DeltaLookup(std::string path, std::string device)
{
    /* Load index - compress to ~41MB of alloced memory */
    FILE* fd = fopen(path.c_str(),"r");
    if (fd==0) {
        printf("Could not open %s for reading.\n", path.c_str());
    }
    assert(fd);
    fseek(fd ,0 ,SEEK_END );
    long size = ftell(fd);
    unsigned int num = (size / sizeof(uint64_t))-1;
    fseek(fd ,0 ,SEEK_SET );
    mBlockIndex = new int[num+1];
    mPrimaryIndex = new uint64_t[num/256];
    size_t alloced = num*sizeof(int)+(num/256)*sizeof(int64_t);
    printf("Allocated %i bytes.\n",alloced);
    mNumBlocks = num;
    assert(mBlockIndex);
    uint64_t end;
    mStepSize = 0xfffffffffffffLL/(num+1);
    int64_t min = 0;
    int64_t max = 0;
    int64_t last = 0;
    for(int bl=0; bl<num; bl++) {
        size_t r = fread(&end,sizeof(uint64_t),1,fd);
        assert(r==1);
        int64_t offset = (end>>12)-last-mStepSize;
        last = end>>12;
        if (offset>max) max = offset;
        if (offset<min) min = offset;
        assert(offset<0x7fffffff);
        assert(offset>-0x7fffffff);
        mBlockIndex[bl]=offset;
        if ((bl&0xff)==0) {
            mPrimaryIndex[bl>>8]=end;
        }
    }
    mBlockIndex[num] = 0x7fffffff; /* for detecting last index */
    printf("%llx %llx %llx\n", min,max,mPrimaryIndex[1]);

    mLowEndpoint = mPrimaryIndex[0];
    size_t r=fread(&mHighEndpoint,sizeof(uint64_t),1,fd);
    assert(r==1);
    fclose(fd);
    mBlockOffset=0ULL;

    mDevice = open(device.c_str(),O_RDONLY);
    assert(mDevice>=0);

    if (!mInitStatics) {
        /* Fill in dencoding tables */
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
        mInitStatics = true;
    }
}

DeltaLookup::~DeltaLookup()
{
    close(mDevice);
    delete [] mBlockIndex;
    delete [] mPrimaryIndex;
}

#define DEBUG_PRINT 0

int DeltaLookup::FindEndpoint(uint64_t end)
{
    if (end<mLowEndpoint) return 0;
    if (end>mHighEndpoint) return 0;

    uint64_t bid = (end>>12) / mStepSize;
    unsigned int bl = ((unsigned int)bid)/256;

    /* Methinks the division has been done by float, and may 
     * have less precision than required
     */
    while (bl && (mPrimaryIndex[bl]>end)) bl--;

    uint64_t here = mPrimaryIndex[bl];
    int count = 0;
    bl = bl *256;
    uint64_t delta = (mStepSize + mBlockIndex[bl+1])<<12;
    while((here+delta)<=end) {
        here+=delta;
        bl++;
        count++;
        delta = (mStepSize + mBlockIndex[bl+1])<<12;
    }

#if DEBUG_PRINT
    printf("%i block (%i)\n", bl, count);
#endif

    lseek(mDevice, ((uint64_t)bl+mBlockOffset)*4096, SEEK_SET );
    size_t r = read(mDevice,mBuffer,4096);
    assert(r==4096);

    unsigned int mBufPos = 0;
    unsigned int mBitBuffer = mBuffer[mBufPos++];
    unsigned int mBitCount = 8;
    unsigned char bits;
    uint64_t index;
    uint64_t tmp;

    /* read generating index for first chain in block */
    READ8();
    tmp = bits;
    READ8();
    tmp = (tmp<<8)|bits;
    READ8();
    tmp = (tmp<<8)|bits;
    READ8();
    tmp = (tmp<<8)|bits;
    READN(2);
    tmp = (tmp<<2)|bits;

#if DEBUG_PRINT
    printf("%llx %llx\n", here, tmp);
#endif

    if (here==end) {
        mHits.push(tmp);
        return 1;
    }

    for(;;) {
        int available = (4096-mBufPos)*8 + mBitCount;
        if (available<51) {
#if DEBUG_PRINT
            printf("End of block (%i bits left)\n", available);
#endif
            break;
        }
        READ8();
        if (bits==0xff) {
            if (available<72) {
#if DEBUG_PRINT
                printf("End of block (%i bits left)\n", available);
#endif
                break;
            }
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
                d2 = bits;
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
        here += delta<<12;
#if DEBUG_PRINT
        printf("%llx %llx\n", here, index);
#endif
        if (here==end) {
           mHits.push(index);
           return 1; 
        }

        if (here>end) {
#if DEBUG_PRINT
            printf("passed: %llx %llx\n", here, end);
#endif
            break;
        }
    }

    return 0;
}


bool DeltaLookup::PopResult(uint64_t & dst)
{
    if (mHits.size()==0) return false;
    dst = mHits.front();
    mHits.pop();
    return true;
}
