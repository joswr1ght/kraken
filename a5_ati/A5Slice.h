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

#ifndef A5SLICE_H
#define A5SLICE_H

#include "A5Ati.h"
#include "CalDevice.h"
#include "CalModule.h"
#include "calcl.h"
#include "cal_ext_counter.h"
#include <sys/time.h>
#include <stdint.h>

typedef struct {
    unsigned int x;
    unsigned int y;
} uint2;

typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned int z;
    unsigned int w;
} uint4;


class AtiA5;

class A5Slice {
public:
    enum wait_state {
        ePopulate,
        eDMAto,
        eKernel,
        eDMAfrom,
    };

    A5Slice(AtiA5* cont, int num, int dp, int rounds, int pipe_mult);
    ~A5Slice();

    void test();

    uint64_t getState( int block, int bit );
    void setState( uint64_t v, int block, int bit );

    uint64_t getStateRev( int block, int bit );
    void setStateRev( uint64_t v, int block, int bit );

    void setRound( int block, int bit, uint64_t v );

    bool tick();
    void flush();

    enum wait_state getWaitState() { return mWaitState; }

    int getNumSlots() { return mNum; }

    void makeAvailable(int num) { mAvailable = num; }
    int getNumJobs() { return mNumJobs; }

    static int getNumDevices();

private:
    void process();
    void populate();

    int mDevNo;
    int mNum;
    int mNumRounds;

    CalDevice* mDev;
    CalModule* mModule;

    CALresource* mConstCount;
    CALresource* mResControl;
    unsigned int* mControl;

    CALresource* mResStateLocal;
    CALresource* mResStateRemote;    
    uint4* mState;

    CALobject mObject;
    CALimage  mImage;

    AtiA5::JobPiece_s* mJobs;
    int mFree;     /* head of free list */
    int mNumJobs;  /* Active jobs       */
    int mAvailable;  /* Allowance from queue */

    int           mIterCount;
    int           mDp;

    /* Controller */
    AtiA5*        mController;

    wait_state    mWaitState;
    CALevent      mEvent;
    CALcontext*   mCtx;
    CALmem*       mMemLocal;
    CALmem        mMemRemote;
    CALcounter    mCounter;
    int           mTicks;
    struct timeval mTvStarted;

    int           mMaxCycles;
};

#endif
