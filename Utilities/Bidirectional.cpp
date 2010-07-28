#include "Bidirectional.h"

#include <stdio.h>
#include <iostream>
#include <list>
#include <assert.h>

Bidirectional::Bidirectional() :
    mPrintCand(true)
{
}

void Bidirectional::FillBack(uint64_t final) {
    uint64_t lfsr1 = final & 0x7ffff;
    uint64_t lfsr2 = (final>>19) & 0x3fffff;
    uint64_t lfsr3 = (final>>41) & 0x7fffff;

    /* precalculate MAX_STEPS backwards clockings of all lfsrs */
    for (int i=0; i<MAX_STEPS; i++) {
      mBack1[i] = lfsr1 & 0x7ffff;
      uint64_t bit = lfsr1 ^ (lfsr1>>18) ^ (lfsr1>>17) ^ (lfsr1>>14);
      lfsr1 = (lfsr1>>1) | ((bit&0x01)<<18);
    }

    for (int i=0; i<MAX_STEPS; i++) {
      mBack2[i] = (lfsr2 & 0x3fffff)<<19;
      uint64_t bit = lfsr2 ^ (lfsr2>>21);
      lfsr2 = (lfsr2>>1) | ((bit&0x01)<<21);
    }

    for (int i=0; i<MAX_STEPS; i++) {
      mBack3[i] = (lfsr3 & 0x7fffff)<<41;
      uint64_t bit = lfsr3 ^ (lfsr3>>22) ^ (lfsr3>>21) ^ (lfsr3>>8);
      lfsr3 = (lfsr3>>1) | ((bit&0x01)<<22);
    }

    /***
     * Verify with MAX_STEPS steps forwards
     *
     * (Only used to verify code correctness)
     **/
    unsigned int wlfsr1 = lfsr1;
    unsigned int wlfsr2 = lfsr2;
    unsigned int wlfsr3 = lfsr3;
    for (int i=0; i<MAX_STEPS; i++) {
        /* Clock the different lfsr */
        unsigned int val = (wlfsr1&0x52000)*0x4a000;
        val ^= wlfsr1<<(31-17);
        wlfsr1 = 2*wlfsr1 | (val>>31);

        val = (wlfsr2&0x300000)*0xc00;
        wlfsr2 = 2*wlfsr2 | (val>>31);

        val = (wlfsr3&0x500080)*0x1000a00;
        val ^= wlfsr3<<(31-21);
        wlfsr3 = 2*wlfsr3 | (val>>31);
    }
    wlfsr1 = wlfsr1 & 0x7ffff;
    wlfsr2 = wlfsr2 & 0x3fffff;
    wlfsr3 = wlfsr3 & 0x7fffff;

    uint64_t cmp = (uint64_t)wlfsr1 | ((uint64_t)wlfsr2<<19) | ((uint64_t)wlfsr3<<41);
    std::cout << std::hex << final << " -> " << cmp << "\n";

    assert(cmp==final);
}


uint64_t  Bidirectional::Forwards(uint64_t start, int steps, unsigned char* out)
{
  unsigned int lfsr1 = start & 0x7ffff;
  unsigned int lfsr2 = (start>>19) & 0x3fffff;
  unsigned int lfsr3 = (start>>41) & 0x7fffff;

  int bits = 0;
  unsigned char byte = 0;
  unsigned char* ch = out;

  for (int i=0; i<steps; i++) {
    /* Majority count */
    int count = ((lfsr1>>8)&0x01);
    count += ((lfsr2>>10)&0x01);
    count += ((lfsr3>>10)&0x01);
    count = count >> 1;

    unsigned int bit = ((lfsr1>>18)^(lfsr2>>21)^(lfsr3>>22))&0x01;
    byte = (byte<<1) | bit;
    
    /* Clock the different lfsr */
    if (((lfsr1>>8)&0x01)==count) {
      unsigned int val = (lfsr1&0x52000)*0x4a000;
      val ^= lfsr1<<(31-17);
      lfsr1 = 2*lfsr1 | (val>>31);
    }
    if (((lfsr2>>10)&0x01)==count) {
      unsigned int val = (lfsr2&0x300000)*0xc00;
      lfsr2 = 2*lfsr2 | (val>>31);
    }
    if (((lfsr3>>10)&0x01)==count) {
      unsigned int val = (lfsr3&0x500080)*0x1000a00;
      val ^= lfsr3<<(31-21);
      lfsr3 = 2*lfsr3 | (val>>31);
    }
    
    bits++;
    if( ((bits&0x7)==0)&&ch ) {
        *ch++ = byte;
        byte = 0;
    }
    
  }

  if( (bits&0x7) && ch ) {
      *ch++ = byte << (8-(bits&0x7));
  }  
  
  lfsr1 = lfsr1 & 0x7ffff;
  lfsr2 = lfsr2 & 0x3fffff;
  lfsr3 = lfsr3 & 0x7fffff;
  
  uint64_t res = (uint64_t)lfsr1 | ((uint64_t)lfsr2<<19) | ((uint64_t)lfsr3<<41);
 
  return res;
}

void Bidirectional::ClockBack(uint64_t final, int steps,
                              int offset1, int offset2, int offset3) {
    if (steps>(MAX_STEPS-2)) return;
    if (steps<=0) return; 
    if ((offset1+offset2+offset3)==0) FillBack( final );

    int todo = steps > 20 ? 20 : steps;

    int limit = 2 * todo; /* Minimum number of clockings */

    for( int i=0; i<=todo; i++ ) {
        for( int j=0; j<=todo; j++ ) {
            for( int k=0; k<=todo; k++) {
                if ((i+j+k)<limit) continue;
                uint64_t test = mBack1[offset1+i] | mBack2[offset2+j]
                    | mBack3[offset3+k];
                uint64_t res = Forwards(test, todo, NULL);
                if (res == final) {
                    int remain = steps - todo;
                    if (remain>0) {
                        /* Recursion */
                        ClockBack( test, remain, offset1+i,
                                   offset2+j, offset3+k );
                    } else {
                        mCandidates.push_back(test);
                        if (mPrintCand) 
                        {
                            std::cout << "Candidate: " << 
                                std::hex << test << "\n";
                        }
                    }
                }
            }
        }
    }
}

bool Bidirectional::PopCandidate(uint64_t& c)
{
    if (mCandidates.size()>0)
    {
        c = mCandidates.back();
        mCandidates.pop_back();
        return true;
    }
    return false;
}

uint64_t Bidirectional::ReverseBits(uint64_t r)
{
    uint64_t r1 = r;
    uint64_t r2 = 0;
    for (int j = 0; j < 64 ; j++ ) {
        r2 = (r2<<1) | (r1 & 0x01);
        r1 = r1 >> 1;
    }
    return r2;
}
