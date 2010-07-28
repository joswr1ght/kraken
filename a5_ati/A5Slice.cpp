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

A5Slice::A5Slice(AtiA5* cont, int dev, int dp, int rounds, int pipe_mult) :
    mNumRounds(rounds),
    mController(cont),
    mState(0),
    mDp(dp),
    mWaitState(ePopulate),
    mTicks(0)
{
    mDevNo = dev;
    mMaxCycles = (1<<mDp)*rounds*10;

    // CAL setup
    assert((dev>=0)&&(dev<CalDevice::getNumDevices()));
    mDev = CalDevice::createDevice(dev);
    assert(mDev);
    mNum = mDev->getDeviceAttribs()->wavefrontSize *
        mDev->getDeviceAttribs()->numberOfSIMD * pipe_mult;

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

    /* Init free list */
    int jobs = mNum * 32;
    mJobs = new AtiA5::JobPiece_s[jobs];
    for(int i=0; i<jobs; i++) {
        mJobs[i].next_free = i-1;
        mJobs[i].idle = true;
    }
    mFree = jobs - 1;
    mNumJobs = 0;

    memset( mControl, 0, mNum*sizeof(unsigned int) );
};

A5Slice::~A5Slice() {
    delete [] mControl;
    delete [] mJobs;

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

/* Set the round index of a particular chain slot */
void A5Slice::setRound( int block, int bit, uint64_t v) {
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

        for( int j=0; j<32; j++) {
            AtiA5::JobPiece_s* job = &mJobs[32*i+j];
            if (!job->idle) {
                job->cycles += mIterCount;
                bool intMerge = job->cycles>mMaxCycles;
                if (intMerge || (1<<j)&todo) {
                    /* Time to change round or evict stuck chain */
                    int round = job->current_round;
                    if ((!intMerge)&&(round<job->end_round)) {
                        uint64_t res = getStateRev(i,j) ^ job->round_func[round];
                        round++;
                        job->current_round = round;
                        uint64_t rfunc = job->round_func[round];
                        setRound(i,j,rfunc);
                        /* redo previous round function */
                        setStateRev(res^rfunc, i,j);
                        /* Compute this round even if at DP */
                        control |= (1<<j);
                    } else {
                        /* Chain completed */
                        uint64_t res = getStateRev(i,j);
                        if (job->end_round==(mNumRounds-1)) {
                            /* Allow partial searches to complete */
                            res = res ^ job->round_func[round];
                        }
                        if (intMerge) {
                            /* Internal merges are reported as an invalid end point */
                            res = 0xffffffffffffffffULL;
                        }
                        job->end_value = res;
                        mController->PushResult(job);
                        /* This item is idle, try to populate with new job */
                        if( mAvailable && mController->PopRequest(job) ) {
                            uint64_t rfunc = job->round_func[job->current_round];
                            setStateRev( job->start_value, i, j );
                            setRound(i,j, rfunc);
                            mAvailable--;
                        } else {
                            /* Add to free list */
                            setStateRev( 0ULL, i, j );
                            setRound(i,j, 0ULL);
                            job->next_free = mFree;
                            job->idle = true;
                            mFree = 32*i+j;
                            mNumJobs--;
                        }
                    }
                }
            }
        }

        mControl[i] = control;
    }

    /* Populate to idle queue */
    while( mAvailable && (mFree>=0)) {
        int i,j;
        i = mFree >> 5;
        j = mFree & 0x1f;
        AtiA5::JobPiece_s* job = &mJobs[mFree];
        if (mController->PopRequest(job) ) {
            mFree = job->next_free;
            uint64_t rfunc = job->round_func[job->current_round];
            setStateRev( job->start_value, i, j );
            setRound(i,j, rfunc);
            mAvailable--;
            mNumJobs++;
        } else {
            /* Something not right */
            break;
        }
    }
}

/* Initialize the pipeline (set to 0) */
void A5Slice::populate() {
    CALuint pitch = 0;
    if (calResMap((CALvoid**)&mState, &pitch, *mResStateRemote, 0) != CAL_RESULT_OK) 
    {
        assert(!"Can't map mConstCount resource");
    }
    for (int i=0; i < mNum ; i++ ) {
        for(int j=0; j < 32; j++) {
            setStateRev( 0ULL, i, j );
            setRound(i,j, 0ULL);
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
