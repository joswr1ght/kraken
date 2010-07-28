#ifndef CALDEVICE_H
#define CALDEVICE_H

#include "cal.h"
#include <list>
#include <map>

class CalDevice {
public:
    static CalDevice* createDevice(int num);
    CALcontext* getContext() {return &mCtx;}

    static int getNumDevices();

    CALresource* resAllocRemote1D( int, CALformat, CALuint f=0 );
    CALresource* resAllocRemote2D( int, int, CALformat, CALuint f=0 );

    CALresource* resAllocLocal1D( int, CALformat, CALuint f=0 );
    CALresource* resAllocLocal2D( int, int, CALformat, CALuint f=0 );

    static void refResource(CALresource*);
    static void unrefResource(CALresource*);

    const CALdeviceinfo* getDeviceInfo() { return &mInfo; }
    const CALdeviceattribs* getDeviceAttribs() { return &mAttribs; }

    ~CalDevice();

private:
    CalDevice(int);
    void freeResource(CALresource*);

    CALdevice mDevice;
    CALcontext mCtx;     /* 1 context pr device */
    int mNumDev;
    CALdeviceinfo mInfo;
    CALdeviceattribs mAttribs;
    std::list<CALresource> mResources;
    static std::map<CALresource*,int> mRefs;
    static std::map<CALresource*,CalDevice*> mOwners;
    bool mHasContext;
};


#endif
