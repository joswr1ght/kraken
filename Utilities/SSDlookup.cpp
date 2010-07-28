#define _FILE_OFFSET_BITS 64

#include "SSDlookup.h"
#include <iostream>
#include <stdio.h>
#include <list>
#include <sys/time.h>
#include <assert.h>
#include <linux/fs.h>
#include <stdlib.h>
#include <stropts.h>
#include <fcntl.h>

SSDlookup::SSDlookup(std::string path, std::string device)
{
    /* Load index */
    FILE* fd = fopen(path.c_str(),"r");
    if (fd==0) {
        printf("Could not open %s for reading.\n", path.c_str());
    }
    assert(fd);
    fseek(fd ,0 ,SEEK_END );
    long size = ftell(fd);
    unsigned int num = size / sizeof(uint64_t);
    fseek(fd ,0 ,SEEK_SET );
    mBlockIndex = new uint64_t[num];
    size_t read = fread(mBlockIndex,sizeof(uint64_t), num, fd);
    assert(read==num);
    fclose(fd);
    mNumBlocks = num;

    int step = 1;
    while (step<num) {
        step = step * 2;
    }
    mFirstStep = step;

    mDevice = open(device.c_str(),O_RDONLY);
    assert(mDevice>=0);

    mData = new uint64_t[512];
}

SSDlookup::~SSDlookup()
{
    close(mDevice);
    delete [] mBlockIndex;
    delete [] mData;
}

#define DEBUG_PRINT 0

int SSDlookup::FindEndpoint(uint64_t end)
{
    uint64_t e = end;
    uint64_t here;

    int pos = mNumBlocks / 2;
    int step = mFirstStep;
    while (step) {
        here = mBlockIndex[pos];
        if (here==e) break;
        step = step / 2;
        if (here<e) {
            pos += step;
            if (pos>=mNumBlocks) pos = mNumBlocks-1;
        } else {
            pos -= step;
            if (pos<0) pos = 0;
        }
#if DEBUG_PRINT
        std::cout << "   " << std::hex << here << " " << e << "\n";
        std::cout << "   pos: " << pos << " (" << step << ")\n";
#endif
    }

    if (mBlockIndex[pos]>e) pos--;
    uint64_t off =  mBlockIndex[pos];

    lseek(mDevice , (uint64_t)pos*4096, SEEK_SET );

    size_t r = read(mDevice,mData,sizeof(uint64_t)*512);

#if DEBUG_PRINT
    std::cout << "searching block, off: " << std::hex << off << " " << e-off << " " << mBlockIndex[pos+1] << "\n";
    std::cout << "   " << std::hex << mData[0] << "\n";
#endif

    pos = 256;
    step = pos;
    while (step) {
        here = off+((mData[pos] & 0x3FFFFFFFULL)<<12);
#if DEBUG_PRINT
        std::cout << "   " << std::hex << here << " " << e << "\n";
        std::cout << "   ind: " << pos << " (" << step << ")\n";
#endif
        if (here==e) break;
        step = step / 2;
        if (here<e) {
            pos += step;
        } else {
            pos -= step;
        }
    }

#if DEBUG_PRINT
    std::cout << "Looking for " << std::hex << end << " " << off << " " << pos << "\n";
#endif

    if (here==e) {
        /* found, push index */
        mHits.push(mData[pos]>>30);
        return 1;
    }

    return 0;
}


bool SSDlookup::PopResult(uint64_t & dst)
{
    if (mHits.size()==0) return false;
    dst = mHits.front();
    mHits.pop();
    return true;
}
