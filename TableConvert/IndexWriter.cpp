#define _FILE_OFFSET_BITS 64

#include "IndexWriter.h"
#include <assert.h>
#include <string.h>

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

IndexWriter::IndexWriter(std::string &path,
                         std::string &index,
                         unsigned int blockSize) :
    mFile(NULL),
    mIndexFile(NULL),
    mBuffer(0),
    mBufPos(0),
    mBitBuffer(0),
    mBitCount(0),
    mIsOK(true),
    mLast(0ULL),
    mBitsWritten(0),
    mChainsWritten(0),
    mLastEnd(0)
{
    unsigned int bits = 512;
    for(int i=0; i < 6 ; i++) {
        if (bits==blockSize) {
            bits = 0; // OK
            break;
        }
        bits = bits * 2;
    }
    if (bits) {
        mIsOK = false;
        return;
    }

    mBlockSize = blockSize;
    mBuffer = new unsigned char[mBlockSize];

    mIndexFile = fopen(index.c_str(),"w");
    if(mIndexFile==NULL) {
        mIsOK = false;
        return;
    }

    uint64_t seek_offset = 0;
    const char* colon = strchr(path.c_str(),':');
    if (colon) {
        sscanf(&colon[1],"%lli",&seek_offset);
        const char* ch = path.c_str();
        path = string(ch, colon-ch);
        printf("seek offset: %lli blocks (%s)\n",seek_offset,path.c_str());
    }
    

    mFile = fopen(path.c_str(),"w");
    if (mFile==NULL) {
        fclose(mIndexFile);
        mIndexFile = NULL;
        mIsOK = false;
        return;
    }

    if (seek_offset) {
        fseeko64(mFile,seek_offset*blockSize,SEEK_SET);
    }

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

IndexWriter::~IndexWriter()
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

    /* Close index file */
    if (mIndexFile) {
        /* Write last chain as last index to denot range of table
         * this entry does not have an accompanying block of data
         */
        fwrite(&mLastEnd,sizeof(uint64_t),1,mIndexFile);
        fclose(mIndexFile);
    }

    printf("%lli chains written, %f bits pr chain.\n",
           mChainsWritten,
           (float)mBitsWritten/(float)mChainsWritten);

    delete [] mBuffer;
}

void IndexWriter::Write(uint64_t endpoint, uint64_t index)
{
    uint64_t data;
    int bout;

    uint64_t delta = endpoint - mLast;
    delta = delta >> 12;
    uint64_t radix = delta>>10;
    mLast = endpoint;
    mLastEnd = endpoint;

    int peek_size = 0;
    if ((radix>=mRadixCount)||(index>=0x200000000ULL)) {
        peek_size = 64 + 8;
    } else {
        peek_size = 33+18+mBits[(unsigned int)radix];
    }
    int available = 8*(mBlockSize-mBufPos)-mBitCount;
    if (peek_size>available) {
        /* Flush & pad with 0xff...  */
        mBitsWritten+=available;
        mBuffer[mBufPos++] = mBitBuffer | (0xff>>mBitCount);
        while (mBufPos<mBlockSize) {
            mBuffer[mBufPos++] = 0xff;
        }
        assert(mFile);
        fwrite(mBuffer,1,mBlockSize,mFile);
        mBitBuffer=0;
        mBitCount = 0;
        mBufPos=0;
    }

    if (mBufPos==0) {
        /* First chain in block gets special treatment */
        /* Just write end point to index file */
        fwrite(&endpoint,sizeof(uint64_t),1,mIndexFile);
        /* And only start index to beginning of block */
        data = index;
        bout = 34;
        WRITE();
        mChainsWritten++;
        return;
    }

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
}
