#ifndef CALMODULE_H
#define CALMODULE_H

#include "CalDevice.h"
#include "calcl.h"
#include <string>
#include <map>

class CalModule {
public:
    CalModule(CALcontext* ctx);
    ~CalModule();

    bool Load(CALimage &image);
    CALmem* Bind(const char* name, CALresource* pRes);
    bool Exec(CALdomain &dom, int groupsize = 0);
    bool Finished();

    CALcontext* GetContext() {return mCtx;}

private:
    void Cleanup();

    CalDevice* mDev;
    CALcontext* mCtx;
    CALmodule mModule;
    CALfunc mEntry;
    bool mLoaded;
    typedef struct {
        CALresource* pRes;
        CALmem       mem;
        CALname      name;
    } stream_t;
    typedef std::map<std::string,stream_t> streamdict_t;
    streamdict_t mStreams;

    bool mRunning;
    CALevent mEvent;
};


#endif
