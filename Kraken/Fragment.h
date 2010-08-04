#ifndef FRAGMENT_H
#define FRAGMENT_H

/**
 * A class that encapsulates a search fragment
 * a frgment is a test for:
 * 1 known plaintext
 * against:
 * 1 table (id)
 * 1 colour (round)
 **/

#include "NcqDevice.h"
#include "DeltaLookup.h"
#include <stdint.h>

class Fragment : public NcqRequestor {
public:
    Fragment(uint64_t plaintext, unsigned int round,
                    DeltaLookup* table, unsigned int advance);
    void processBlock(const void* pDataBlock);

    void setBitPos(int pos) {mBitPos=pos;}
    int getBitPos() {return mBitPos;}

    void setJobNum(unsigned int j) {mJobNum=j;}
    unsigned int getJobNum() {return mJobNum;}
    void setClientId(int id) {mClientId=id;}
    int getClientId() {return mClientId;}

    void handleSearchResult(uint64_t result, int start_round);

private:
    uint64_t mKnownPlaintext;
    unsigned int mNumRound;
    unsigned int mAdvance;
    DeltaLookup* mTable;
    int mBitPos;
    int mState;
    unsigned int mJobNum;
    int mClientId;

    uint64_t mEndpoint;
    uint64_t mBlockStart;
    uint64_t mStartIndex;
};

#endif
