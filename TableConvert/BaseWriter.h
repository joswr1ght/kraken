#ifndef BASEWRITER_H
#define BASEWRITER_H

#include <stdint.h>

class BaseWriter {
public:
    BaseWriter() {};
    virtual ~BaseWriter() {};
    virtual void Write(uint64_t endpoint, uint64_t index) = 0;
    virtual bool isOK() = 0;
};

#endif
