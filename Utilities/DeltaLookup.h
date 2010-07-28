#ifndef DELTA_LOOKUP_H
#define DELTA_LOOKUP_H
#include <queue>
#include <string>
#include <stdint.h>

class DeltaLookup {
public:
    DeltaLookup(std::string index, std::string device);
    ~DeltaLookup();
    int FindEndpoint(uint64_t end);

    bool PopResult(uint64_t & dst);
    void SetBlockOffset(uint64_t bo) {mBlockOffset=bo;}

private:
    int mDevice;

    std::queue<uint64_t> mHits;

    int* mBlockIndex;
    uint64_t* mPrimaryIndex;
    int mNumBlocks;
    int64_t mStepSize; // avarage step pr block
    unsigned char mBuffer[4096];
    uint64_t mLowEndpoint;
    uint64_t mHighEndpoint;
    uint64_t mBlockOffset;

    static bool mInitStatics;
    static unsigned short mBase[256];
    static unsigned char mBits[256];
};

#endif
