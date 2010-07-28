#ifndef ADVANCE_H
#define ADVANCE_H

#include <stdint.h>

class Advance {
public:
    Advance(unsigned int id, unsigned int size);
    ~Advance();

    const uint64_t* getAdvances() {return mAdvances;}
    const uint32_t* getRFtable() {return mRFtable;}

private:
    uint64_t AdvanceRFlfsr(uint64_t v);
    uint64_t ReverseBits(uint64_t r);
    uint64_t* mAdvances;
    uint32_t* mRFtable;
};

#endif
