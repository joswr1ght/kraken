#ifndef BASEREADER_H
#define BASEREADER_H

#include <stdint.h>

class BaseReader {
public:
    BaseReader() {};
    virtual ~BaseReader() {};
    virtual bool Read(uint64_t& endpoint, uint64_t& index) = 0;
    virtual bool isOK() = 0;
};

#endif
