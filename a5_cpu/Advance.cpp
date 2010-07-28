#include "Advance.h"

/* Reverse bit order of an unsigned 64 bits int */
uint64_t Advance::ReverseBits(uint64_t r)
{
  uint64_t r1 = r;
  uint64_t r2 = 0;
  for (int j = 0; j < 64 ; j++ ) {
    r2 = (r2<<1) | (r1 & 0x01);
    r1 = r1 >> 1;
  }
  return r2;
}

/* Get next number from lfsr random number generator */
uint64_t Advance::AdvanceRFlfsr(uint64_t v)
{
  for (int i = 0; i < 64; i++) {
    uint64_t fb = (
		   ~(
		     ((v >> 63) & 1) ^
		     ((v >> 62) & 1) ^
		     ((v >> 60) & 1) ^
		     ((v >> 59) & 1)
		     )
		   ) & 1;
    v <<= 1;
    v |= fb;
  }
  return v;
}

Advance::Advance(unsigned int id, unsigned int size)
{
    mAdvances = new uint64_t[size];
    mRFtable = new uint32_t[2*size];

    uint64_t r = 0;
    for (int i=0; i<id; i++) r = AdvanceRFlfsr(r);

    for (int i=0; i<size; i++) {
        uint64_t r2 = ReverseBits(r);
        mAdvances[i] = r;
        mRFtable[2*i]   = (unsigned int)(r2);
        mRFtable[2*i+1] = (unsigned int)(r2>>32);
        r = AdvanceRFlfsr(r);
    }


}

Advance::~Advance()
{
    delete [] mAdvances;
    delete [] mRFtable;
}
