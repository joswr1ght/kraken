/***************************************************************
 * A5/1 Chain generator.
 *
 * Copyright 2009. Frank A. Stevenson. All rights reserved.
 *
 * Permission to distribute, modify and copy is granted to the
 * TMTO project, currently hosted at:
 * 
 * http://reflextor.com/trac/a51
 *
 * Code may be modifed and used, but not distributed by anyone.
 *
 * Request for alternative licencing may be directed to the author.
 *
 * All (modified) copies of this source must retain this copyright notice.
 *
 *******************************************************************/

#include "A5Slice.h"
#include <assert.h>
#include <sys/time.h>
#include <iostream>
#include <string.h>
#include <stdio.h>

#include "kernelLib.h"

#define DISASSEMBLE 0

#if DISASSEMBLE
static FILE* gDisFile;

static void logger(const char*msg)
{
    if (gDisFile) {
        fwrite(msg,strlen(msg),1,gDisFile);
    }
}
#endif


PFNCALCTXCREATECOUNTER Ext_calCtxCreateCounter = 0;
PFNCALCTXBEGINCOUNTER Ext_calCtxBeginCounter = 0;
PFNCALCTXENDCOUNTER Ext_calCtxEndCounter = 0;
PFNCALCTXDESTROYCOUNTER Ext_calCtxDestroyCounter = 0;
PFNCALCTXGETCOUNTER Ext_calCtxGetCounter = 0;

A5Slice::A5Slice(BrookA5* cont, int dev, int dp, int rounds, int advance) :
    mNumRounds(rounds),
    mController(cont),
    mState(0),
    mDp(dp),
    mWaitState(ePopulate),
    mTicks(0)
{
    mDevNo = dev;

    // CAL setup
    assert((dev>=0)&&(dev<CalDevice::getNumDevices()));
    mDev = CalDevice::createDevice(dev);
    assert(mDev);
    mNum = mDev->getDeviceAttribs()->wavefrontSize *
        mDev->getDeviceAttribs()->numberOfSIMD * 4;

    printf("Num threads %i\n", mNum );

    unsigned int dim = mNum;
    unsigned int dim32 = 32*mNum;

    mConstCount = mDev->resAllocLocal1D(1,CAL_FORMAT_UINT_1,0);
    mResControl = mDev->resAllocLocal1D(dim,CAL_FORMAT_UINT_1,0);
    mResStateLocal = mDev->resAllocLocal1D(dim32,CAL_FORMAT_UINT_4,
                                           CAL_RESALLOC_GLOBAL_BUFFER);
    mResStateRemote = mDev->resAllocRemote1D(dim32,CAL_FORMAT_UINT_4,
                       CAL_RESALLOC_CACHEABLE|CAL_RESALLOC_GLOBAL_BUFFER);

    /* Lazy check to ensure that memory has been allocated */
    assert(mConstCount);
    assert(mResControl);
    assert(mResStateLocal);
    assert(mResStateRemote);

    unsigned char* myKernel = getKernel(dp);
    assert(myKernel);

    if (calclCompile(&mObject, CAL_LANGUAGE_IL, (const CALchar*)myKernel,
                     mDev->getDeviceInfo()->target) != CAL_RESULT_OK) {
        assert(!"Compilation failed.");
    }

    freeKernel(myKernel);
    myKernel = NULL;

    if (calclLink (&mImage, &mObject, 1) != CAL_RESULT_OK) {
        assert(!"Link failed.");
    }

#if DISASSEMBLE
    gDisFile = fopen("disassembly.txt","w");
    calclDisassembleImage(mImage, logger);
    fclose(gDisFile);
#endif

    mCtx = mDev->getContext();
    mModule = new CalModule(mCtx);
    if (mModule==0) {
        assert(!"Could not create module");
    }

    if (!mModule->Load(mImage)) {
        assert(!"Could not load module image");
    }

    unsigned int *dataPtr = NULL;
    CALuint pitch = 0;
    mIterCount = (1<<dp)/10;
    if (mIterCount>128) mIterCount = 128;
    mIterCount = 256;
    if (calResMap((CALvoid**)&dataPtr, &pitch, *mConstCount, 0) != CAL_RESULT_OK) 
    {
        assert(!"Can't map mConstCount resource");
    }
    *dataPtr = mIterCount;   /* Number of rounds */
    printf("Running %i rounds pr kernel invocation.\n", *dataPtr);
    calResUnmap(*mConstCount);

    mModule->Bind( "cb0", mConstCount );
    mModule->Bind( "i0", mResControl );
    mMemLocal = mModule->Bind( "g[]", mResStateLocal );

    if (calCtxGetMem(&mMemRemote, *mCtx, *mResStateRemote) != CAL_RESULT_OK) {
        printf("Error calCtxGetMem (%s).\n", calGetErrorString());
        assert(!"Could not add memory to context.");
    };

    if (!Ext_calCtxCreateCounter) {
        calExtGetProc( (CALvoid**)&Ext_calCtxCreateCounter, CAL_EXT_COUNTERS,
                       "calCtxCreateCounter");
    }
    Ext_calCtxCreateCounter( &mCounter, *mCtx, CAL_COUNTER_IDLE );
    if (!Ext_calCtxBeginCounter) {
        calExtGetProc( (CALvoid**)&Ext_calCtxBeginCounter, CAL_EXT_COUNTERS,
                       "calCtxBeginCounter");
    }
    Ext_calCtxBeginCounter( *mCtx, mCounter );
    if (!Ext_calCtxEndCounter) {
        calExtGetProc( (CALvoid**)&Ext_calCtxEndCounter, CAL_EXT_COUNTERS,
                       "calCtxEndCounter");
    }
    if (!Ext_calCtxDestroyCounter) {
        calExtGetProc( (CALvoid**)&Ext_calCtxDestroyCounter, CAL_EXT_COUNTERS,
                       "calCtxDestroyCounter");
    }
    if (!Ext_calCtxGetCounter) {
        calExtGetProc( (CALvoid**)&Ext_calCtxGetCounter, CAL_EXT_COUNTERS,
                       "calCtxGetCounter");
    }

    mControl = new unsigned int[mNum];
    memset( mControl, 0, mNum*sizeof(unsigned int) );

    mRoundTable = new uint64_t[mNumRounds];
    mRoundNo = new unsigned int[32*mNum];
    memset(mRoundNo, 0, 32*mNum*sizeof(unsigned int));

    mStartPoints = new uint64_t[32*mNum];
    mStartRounds = new int[32*mNum];    
    mCycleCount  = new int[32*mNum];    

    /* Init tables */
    uint64_t rnd = 0;
    for (int i=0; i<advance; i++) {
        rnd = BrookA5::AdvanceRFlfsr( rnd );
    }
    for (int i=0; i<rounds; i++) {
        mRoundTable[i] = rnd;
        // std::cout << std::hex << "Rf: " << rnd << "\n";
        rnd = BrookA5::AdvanceRFlfsr( rnd );
    }
};

A5Slice::~A5Slice() {
    delete [] mControl;
    delete [] mRoundTable;
    delete [] mRoundNo;
    delete [] mStartPoints;
    delete [] mStartRounds;
    delete [] mCycleCount;

    Ext_calCtxEndCounter(*mCtx,mCounter);
    Ext_calCtxDestroyCounter( *mCtx, mCounter );

    calCtxReleaseMem(*mCtx, mMemRemote);
    CalDevice::unrefResource(mConstCount);
    CalDevice::unrefResource(mResControl);
    CalDevice::unrefResource(mResStateLocal);
    CalDevice::unrefResource(mResStateRemote);

    delete mModule;
    calclFreeImage(mImage);
    calclFreeObject(mObject);
    delete mDev;
}

/* Get the state of a chain across the slices */
uint64_t A5Slice::getState( int block, int bit ) {
    uint64_t res = 0;
    unsigned int mask = 1 << bit;

    for(int i=0; i<16; i++) {
        unsigned int val = mState[32*block+i].x & mask;
        res = (res << 1) | (val >> bit);
        val = mState[32*block+i].y & mask;
        res = (res << 1) | (val>> bit);
        val = mState[32*block+i].z & mask;
        res = (res << 1) | (val>> bit);
        val = mState[32*block+i].w & mask;
        res = (res << 1) | (val>> bit);
    }

    return res;
}

/* Get the state of a chain across the slices reversed (sane) */
uint64_t A5Slice::getStateRev( int block, int bit ) {
    uint64_t res = 0;
    unsigned int mask = 1 << bit;

    for(int i=0; i<16; i++) {
        uint64_t val = mState[32*block+i].x & mask;
        res = (res >> 1) | ((val >> bit)<<63);
        val = mState[32*block+i].y & mask;
        res = (res >> 1) | ((val >> bit)<<63);
        val = mState[32*block+i].z & mask;
        res = (res >> 1) | ((val >> bit)<<63);
        val = mState[32*block+i].w & mask;
        res = (res >> 1) | ((val >> bit)<<63);
    }

    return res;
}

/* Set the state of a chain across the slices */
void A5Slice::setState( uint64_t v, int block, int bit ) {
    unsigned int mask= 1 << bit;
    unsigned int invmask = ~mask;
    unsigned int b;

    for(int i=0; i<16; i++) {
        unsigned int val = mState[32*block+i].x & invmask;
        b = (v>>63) & 1;
        val = val | (b<<bit);
        v = v << 1;
        mState[32*block+i].x = val;

        val = mState[32*block+i].y & invmask;
        b = (v>>63) & 1;
        val = val | (b<<bit);
        v = v << 1;
        mState[32*block+i].y = val;

        val = mState[32*block+i].z & invmask;
        b = (v>>63) & 1;
        val = val | (b<<bit);
        v = v << 1;
        mState[32*block+i].z = val;

        val = mState[32*block+i].w & invmask;
        b = (v>>63) & 1;
        val = val | (b<<bit);
        v = v << 1;
        mState[32*block+i].w = val;
    }
}

/* Get the state of a chain across the slices, reversed (sane) */
void A5Slice::setStateRev( uint64_t v, int block, int bit ) {
    unsigned int mask= 1 << bit;
    unsigned int invmask = ~mask;
    unsigned int b;

    for(int i=0; i<16; i++) {
        unsigned int val = mState[32*block+i].x & invmask;
        b = v & 1;
        val = val | (b<<bit);
        v = v >> 1;
        mState[32*block+i].x = val;

        val = mState[32*block+i].y & invmask;
        b = v & 1;
        val = val | (b<<bit);
        v = v >> 1;
        mState[32*block+i].y = val;

        val = mState[32*block+i].z & invmask;
        b = v & 1;
        val = val | (b<<bit);
        v = v >> 1;
        mState[32*block+i].z = val;

        val = mState[32*block+i].w & invmask;
        b = v & 1;
        val = val | (b<<bit);
        v = v >> 1;
        mState[32*block+i].w = val;
    }
}

/* Get the round index of a particular chain slot */
unsigned int A5Slice::getRound( int block, int bit ) {
    return mRoundNo[ 32*block + bit ];
}

/* Set the round index of a particular chain slot */
void A5Slice::setRound( int block, int bit, unsigned int round ) {
    mRoundNo[ 32*block + bit ] = round;
    uint64_t v = mRoundTable[round];
    unsigned int mask= 1 << bit;
    unsigned int invmask = ~mask;
    unsigned int b;

    for(int i=0; i<16; i++) {
        uint4 rvec = mState[block*32+16+i];

        unsigned int val = rvec.x & invmask;
        b = v & 1;
        val = val | (b<<bit);
        v = v >> 1;
        rvec.x = val;

        val = rvec.y & invmask;
        b = v & 1;
        val = val | (b<<bit);
        v = v >> 1;
        rvec.y = val;

        val = rvec.z & invmask;
        b = v & 1;
        val = val | (b<<bit);
        v = v >> 1;
        rvec.z = val;

        val = rvec.w & invmask;
        b = v & 1;
        val = val | (b<<bit);
        v = v >> 1;
        rvec.w = val;

        mState[block*32+16+i] = rvec;
    }
}

/* CPU processes the tricky bits (switching rounds) after the
 * GPU has done its part
 */
void A5Slice::process()
{
    // printf("Process\n");

    for (int i=0; i<mNum; i++) {
        unsigned int control = 0;
        unsigned int todo = 0;
        unsigned int* dp_bits = (unsigned int*)&mState[32*i];
        unsigned int* rf_bits = (unsigned int*)&mState[32*i+16];
        for (int j=0; j<mDp; j++) {
            todo |= dp_bits[j]^rf_bits[j];
        }
        todo = ~todo;
        if (todo) {
            // printf("* %08x\n", todo );
            for( int j=0; j<32; j++) {
                mCycleCount[32*i+j] += mIterCount;
                bool intMerge =  (mCycleCount[32*i+j]>3000000);
                if (intMerge || (1<<j)&todo) {
                    int round = getRound(i,j);
                    if ((!intMerge)&&(round<(mNumRounds-1))) {
                        uint64_t res = getStateRev(i,j) ^ mRoundTable[round];
                        // std::cout << "round " << i << " " << j << " " << std::hex << res << " " << round << "\n";
                        setRound(i,j,round+1);
                        /* redo previous round function */
                        // setStateRev(res^mRoundTable[round+1], i,j);
                        setStateRev(res^mRoundTable[round+1], i,j);
                        /* compute even if 0 */
                        control |= (1<<j);
                    } else {
                        uint64_t res = getStateRev(i,j)^mRoundTable[round];                    
                        if (intMerge) {
                            res = 0xffffffffffffffffULL;
                            std::cout << "Internal merge:" << std::hex << mStartPoints[32*i+j] <<
                                "  " << mCycleCount[32*i+j] << "\n";
                            std::cout << "index: " << i << " " << j <<"\n";
                        }
                        // std::cout << "finished " << i << " " << j << " " << std::hex << res << " " << round << mCycleCount[32*i+j] << "\n";
                        mController->PushResult(mStartPoints[32*i+j], res, mStartRounds[32*i+j]);

                        uint64_t start;
                        unsigned int start_round;

                        while (!mController->PopRequest( start, start_round )) {
                            usleep(25000);
                        }

                        setStateRev( start, i, j );
                        setRound(i,j,start_round);

                        mStartPoints[32*i+j] = start;
                        mStartRounds[32*i+j] = start_round;
                        mCycleCount[32*i+j] = 0;
                    }
                }
            }
        } else {
            /* Just add the cycle count */
            for( int j=0; j<32; j++) {
                mCycleCount[32*i+j] += mIterCount;
            }
        }

        mControl[i] = control;
    }

}

/* Initial population of the pipeline */
void A5Slice::populate() {
    printf("Populate\n");
 
    CALuint pitch = 0;
    if (calResMap((CALvoid**)&mState, &pitch, *mResStateRemote, 0) != CAL_RESULT_OK) 
    {
        assert(!"Can't map mConstCount resource");
    }

    for (int i=0; i < mNum ; i++ ) {
        for(int j=0; j < 32; j++) {
            uint64_t start;
            unsigned int round;

            while (!mController->PopRequest( start, round )) {
                usleep(25000);
            }

            setStateRev( start, i, j );
            setRound(i,j,round);

            mStartPoints[32*i+j] = start;
            mStartRounds[32*i+j] = round;
            mCycleCount[32*i+j] = 0;
        }
        mControl[i] = 0;
    }
    calResUnmap(*mResStateRemote);
    mState = 0;
}


bool A5Slice::tick()
{
    CALresult res;

    switch(mWaitState) {
    case ePopulate:
        populate();
        res = calMemCopy(&mEvent, *mCtx, mMemRemote, *mMemLocal, 0);
        if (res!=CAL_RESULT_OK)
        {
            printf("Error %i %p : %s\n", res, mMemLocal, calGetErrorString());
            assert(!"Could not initiate memory copy");
        }
        calCtxIsEventDone(*mCtx, mEvent);
        mWaitState = eDMAto;
        break;
    case eDMAto:
        if (calCtxIsEventDone(*mCtx, mEvent)==CAL_RESULT_PENDING) return false;
        {
            unsigned int *dataPtr = NULL;
            CALuint pitch = 0;
            if (calResMap((CALvoid**)&dataPtr, &pitch, *mResControl, 0) != CAL_RESULT_OK) 
            {
                assert(!"Can't map mResControl resource");
            }
            memcpy( dataPtr, mControl, mNum*sizeof(unsigned int));
            calResUnmap(*mResControl);

            CALdomain domain = {0, 0, mNum, 1};
            gettimeofday(&mTvStarted, NULL);
            if (!mModule->Exec(domain,64)) {
                assert(!"Could not execute module.");
            }
        }
        mModule->Finished();
        calCtxFlush(*mCtx);
        mWaitState = eKernel;
        break;
    case eKernel:
        if (!mModule->Finished()) return false;
        {
            struct timeval tv2;
            gettimeofday(&tv2, NULL);
            unsigned long diff = 1000000*(tv2.tv_sec-mTvStarted.tv_sec);
            diff += tv2.tv_usec-mTvStarted.tv_usec;
            // printf("Exec() took %i usec\n",(unsigned int)diff);
        }
        if (calMemCopy(&mEvent, *mCtx, *mMemLocal, mMemRemote, 0)!=CAL_RESULT_OK)
        {
            printf("Error %s\n", calGetErrorString());
            assert(!"Could not initiate memory copy");
        }
        calCtxIsEventDone(*mCtx, mEvent);
        mWaitState = eDMAfrom;
        break;
    case eDMAfrom:
        if (calCtxIsEventDone(*mCtx, mEvent)==CAL_RESULT_PENDING) return false;
        {
            struct timeval tv1;
            struct timeval tv2;
            gettimeofday(&tv1, NULL);

            CALuint pitch = 0;
            if (calResMap((CALvoid**)&mState, &pitch, *mResStateRemote, 0) != CAL_RESULT_OK) 
            {
                assert(!"Can't map mConstCount resource");
            }

            // Do the actual CPU processing
            process();

            calResUnmap(*mResStateRemote);
            mState = 0;
            gettimeofday(&tv2, NULL);
            unsigned long diff = 1000000*(tv2.tv_sec-tv1.tv_sec);
            diff += tv2.tv_usec-tv1.tv_usec;
            // printf("process() took %i usec\n",diff);
        }
        if (calMemCopy(&mEvent, *mCtx, mMemRemote, *mMemLocal, 0)!=CAL_RESULT_OK)
        {
            printf("Error %s\n", calGetErrorString());
            assert(!"Could not initiate memory copy");
        }
        calCtxIsEventDone(*mCtx, mEvent);
        mWaitState = eDMAto;
        break;
    default:
        assert(!"State error");
    };

#if 0
    /* Ticks that has performed any action make it to here */
    mTicks++;
    if (mTicks%100 == 0) {
        CALfloat perf;
        // Measure performance
        Ext_calCtxEndCounter( *mCtx, mCounter );
        Ext_calCtxGetCounter( &perf, *mCtx, mCounter );
        Ext_calCtxBeginCounter( *mCtx, mCounter );
        printf("Perf %i: %f\n", mDevNo, perf);
    }
#endif
    /* report that something was actually done */
    return true;
}

void A5Slice::flush()
{
    calCtxFlush(*mCtx);
}
