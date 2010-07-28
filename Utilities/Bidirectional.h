#ifndef BIDERECTIONAL_H
#define BIDERECTIONAL_H

#include <list>
#include <stdint.h>

#define MAX_STEPS 250

class Bidirectional {
public:
  Bidirectional();

  void ClockBack(uint64_t final, int steps, int offset1=0, int offset2=0, int offset3=0);

  uint64_t Forwards(uint64_t start, int steps, unsigned char* out);
  static uint64_t ReverseBits(uint64_t r);
  bool PopCandidate(uint64_t& c);

  void doPrintCand(bool d) {mPrintCand=d;}

private:
  void FillBack(uint64_t final);

  std::list<uint64_t> mCandidates;
  uint64_t mBack1[MAX_STEPS];
  uint64_t mBack2[MAX_STEPS];
  uint64_t mBack3[MAX_STEPS];
  bool mPrintCand;
};

#endif
