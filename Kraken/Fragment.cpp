#include <stdio.h>
#include "Fragment.h"
#include "Kraken.h"
#include "../a5_cpu/A5CpuStubs.h"
#include "../a5_ati/A5AtiStubs.h"

static uint64_t kr02_whitening(uint64_t key)
{
    int i;
    uint64_t white = 0;
    uint64_t bits = 0x93cbc4077efddc15ULL;
    uint64_t b = 0x1;
    while (b) {
        if (b & key) {
            white ^= bits;
        }
        bits = (bits<<1)|(bits>>63);
        b = b << 1;
    }
    return white;
}

static uint64_t kr02_mergebits(uint64_t key)
{
    uint64_t r = 0ULL;
    uint64_t b = 1ULL;
    unsigned int i;

    for(i=0;i<64;i++) {
        if (key&b) {
            r |= 1ULL << (((i<<1)&0x3e)|(i>>5));
        }
        b = b << 1;
    }
    return r;
}

void ApplyIndexFunc(uint64_t& start_index, int bits)
{
    uint64_t w = kr02_whitening(start_index);
    start_index = kr02_mergebits((w<<bits)|start_index);
}

Fragment::Fragment(uint64_t plaintext, unsigned int round,
                   DeltaLookup* table, unsigned int advance) :
    mKnownPlaintext(plaintext),
    mNumRound(round),
    mAdvance(advance),
    mTable(table),
    mState(0),
    mJobNum(0),
    mClientId(0),
    mEndpoint(0),
    mBlockStart(0),
    mStartIndex(0)
{
}

void Fragment::processBlock(const void* pDataBlock)
{
    int res = mTable->CompleteEndpointSearch(pDataBlock, mBlockStart,
                                             mEndpoint, mStartIndex);

    if (res) {
        /* Found endpoint */
        // printf("Found: %llx %llx\n", mEndpoint, mStartIndex);
        uint64_t search_rev = mStartIndex;
        ApplyIndexFunc(search_rev, 34);
        if (Kraken::getInstance()->isUsingAti()) {
            if (mNumRound) {
                int res = A5AtiSubmitPartial(search_rev, mNumRound, mAdvance, this);
                if (res<0) printf("Fail\n");
                mState = 3;
            } else {
                A5CpuKeySearch(search_rev, mKnownPlaintext, 0, mNumRound+1, mAdvance, this);
                mState = 2;
            }
        } else {
            A5CpuKeySearch(search_rev, mKnownPlaintext, 0, mNumRound+1, mAdvance, this);
            mState = 2;
        }
    } else {
        /* not found */
        // printf("Not found: %llx\n", mEndpoint);
        return Kraken::getInstance()->removeFragment(this);
    }
}

void Fragment::handleSearchResult(uint64_t result, int start_round)
{
    // printf("handle %llx %i %i\n", result, start_round, mState);
    // printf("*");
    // fflush(stdout);
    if (mState==0) {
        mEndpoint = result;
        mTable->StartEndpointSearch(this, mEndpoint, mBlockStart);
        mState = 1;
        if (mBlockStart==0ULL) {
            /* Endpoint out of range */
            return Kraken::getInstance()->removeFragment(this);
        }
    } else if (mState==2) {
        if (start_round<0) {
            /* Found */
            char msg[128];
            snprintf(msg,128,"Found %016llx @ %i  #%i  (table:%i)\n", result, mBitPos, mJobNum, mAdvance);
            printf("%s",msg);
            Kraken::getInstance()->reportFind(string(msg),mClientId);
        }
        return Kraken::getInstance()->removeFragment(this);
    } else {
        /* We are here because of a partial GPU search */
        /* search final round with CPU */
        A5CpuKeySearch(result, mKnownPlaintext, mNumRound-1, mNumRound+1, mAdvance, this);
        mState = 2;
    }
}
