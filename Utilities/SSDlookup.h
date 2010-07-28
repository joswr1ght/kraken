#ifndef SSD_LOOKUP_H
#define SSD_LOOKUP_H
#include <queue>
#include <string>
#include <stdint.h>

class SSDlookup {
public:
    SSDlookup(std::string index, std::string device);
  ~SSDlookup();
  int FindEndpoint(uint64_t end);

  bool PopResult(uint64_t & dst);

private:
  int mDevice;

  std::queue<uint64_t> mHits;

  uint64_t* mBlockIndex;
  int mNumBlocks;
  int mFirstStep;
  uint64_t* mData;
};

#endif
