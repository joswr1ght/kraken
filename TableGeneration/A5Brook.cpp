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

#include "A5Brook.h"
#include <stdio.h>
#include <sys/time.h>
#include "A5Slice.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;


/**
 * Construct an instance of A5 Brook searcher
 * create tables and streams
 */
BrookA5::BrookA5(int advance, int max_rounds, int condition, int numcores)
{
    mAdvance = advance;
    mCondition = condition;
    mMaxRound = max_rounds;
    mNumCores = numcores;
    mPiplineSize = 0;

    /* Init semaphore */
    sem_init( &mMutex, 0, 1 );

    /* Start worker thread */
    mRunning = true;
    pthread_create(&mThread, NULL, thread_stub, (void*)this);
}

void* BrookA5::thread_stub(void* arg)
{
    if (arg) {
        BrookA5* a5 = (BrookA5*)arg;
        a5->Process();
    }
    return NULL;
}

/**
 * Destroy an instance of A5 Brook searcher
 * delete tables and streams
 */
BrookA5::~BrookA5()
{
    /* stop worker thread */
    mRunning = false;
    pthread_join(mThread, NULL);
    
    sem_destroy(&mMutex);
}
  
int  BrookA5::Submit(uint64_t start_value, unsigned int start_round)
{
    if (start_round>=mMaxRound) return -1;

    int size = 0;
    sem_wait(&mMutex);
    mInputStart.push(start_value);
    mInputRound.push(start_round);
    size = mInputRound.size();
    sem_post(&mMutex);
    return size;
}

int  BrookA5::SubmitPartial(uint64_t start_value, unsigned int stop_round)
{
    if (stop_round>=mMaxRound) return -1;

    int size = 0;
    sem_wait(&mMutex);
    mInputStart.push(start_value);
    mInputRound.push(0);
    size = mInputRound.size();
    sem_post(&mMutex);
    return size;
}
  
bool BrookA5::PopResult(uint64_t& start_value,
			uint64_t& stop_value, int& start_round)
{
    bool res = false;
    sem_wait(&mMutex);
    if (mOutput.size()>0) {
        res = true;
        pair<uint64_t,uint64_t> res = mOutput.front();
        mOutput.pop();
        start_value = res.first;
        stop_value = res.second;
        start_round = mOutputStartRound.front();
        mOutputStartRound.pop();
    }
    sem_post(&mMutex);
    return res;
}

bool BrookA5::PopRequest(uint64_t& start_value, unsigned int& start_round)
{
    bool res = false;
    sem_wait(&mMutex);
    if (mInputStart.size()>0) {
        res = true;
        start_value = mInputStart.front();
        mInputStart.pop();
        start_round = mInputRound.front();
        mInputRound.pop();
    }
    sem_post(&mMutex);
    return res;
}

void BrookA5::PushResult(uint64_t start_value, uint64_t stop_value, int start_round)
{
    sem_wait(&mMutex);
    mOutput.push( pair<uint64_t,uint64_t>(start_value,stop_value) );
    mOutputStartRound.push(start_round);
    sem_post(&mMutex);
}

bool BrookA5::PipelineInfo(int &length)
{
    printf("Query pipesize: %i\n", mPiplineSize);
    if (mPiplineSize<=0) {
        return false;
    }
    length = mPiplineSize;
    return true;
}

void BrookA5::Process(void)
{
    struct timeval tStart;
    struct timeval tEnd;

    int core = mNumCores;
    printf("Running on %i GPUs\n", core);
    
    int numSlices = mNumCores;
    int pipes = 0;

    A5Slice* slices[numSlices];
    
    for( int i=0; i<numSlices ; i++ ) {
        slices[i] = new A5Slice( this, i%mNumCores, mCondition, mMaxRound, mAdvance );
        pipes += 32*slices[i]->getNumSlots();
    }

    /* update after everything is created, compiled and linked */
    mPiplineSize = pipes;    

    for(;;) {
        bool newCmd = false;
        for( int i=0; i<numSlices ; i++ ) {
            newCmd |= slices[i]->tick();
        }

        if (!newCmd) {
            /* In wait state, ease up on the CPU in our busy loop */
            // usleep(0);
        }

        if (!mRunning) break;
    }
    
    for( int i=0; i<numSlices ; i++ ) {
        delete slices[i];
    }
}

/* Reverse bit order of an unsigned 64 bits int */
uint64_t BrookA5::ReverseBits(uint64_t r)
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
uint64_t BrookA5::AdvanceRFlfsr(uint64_t v)
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


/* Stubs for shared library - exported without name mangling */

extern "C" {

static class BrookA5* a5Instance = 0;

bool DLL_PUBLIC A5BrookInit(int advance, int max_rounds, int condition,
                            int numcores)
{
  if (a5Instance) return false;
  a5Instance = new BrookA5(advance, max_rounds, condition, numcores);
  return true;
}

bool DLL_PUBLIC A5BrookPipelineInfo(int &length)
{
    if (a5Instance) {
        return a5Instance->PipelineInfo(length);
    }
    return false;
}

int  DLL_PUBLIC A5BrookSubmit(uint64_t start_value, unsigned int start_round)
{
  if (a5Instance) {
    return a5Instance->Submit(start_value, start_round);
  }
  return -1; /* Error */
}

bool DLL_PUBLIC A5BrookPopResult(uint64_t& start_value, uint64_t& stop_value, int& start_round)
{
  if (a5Instance) {
    return a5Instance->PopResult(start_value, stop_value, start_round);
  }
  return false; /* Nothing popped */ 
}

void DLL_PUBLIC A5BrookShutdown()
{
  delete a5Instance;
  a5Instance = NULL;
}

static uint64_t kr02_whitening(uint64_t key)
{
    int i;
    uint64_t white = 0;
    uint64_t bits = 0x93cbc4077efddc15ULL;
    uint64_t b = 0x1;
    while (b) {
        if (b & key) {
            white ^= bits;
        }
        bits = (bits<<1)|(bits>>63);
        b = b << 1;
    }
    return white;
}

static uint64_t kr02_mergebits(uint64_t key)
{
    uint64_t r = 0ULL;
    uint64_t b = 1ULL;
    unsigned int i;

    for(i=0;i<64;i++) {
        if (key&b) {
            r |= 1ULL << (((i<<1)&0x3e)|(i>>5));
        }
        b = b << 1;
    }
    return r;
}


void DLL_PUBLIC ApplyIndexFunc(uint64_t& start_index, int bits)
{
    uint64_t w = kr02_whitening(start_index);
    start_index = kr02_mergebits((w<<bits)|start_index);
}

int DLL_PUBLIC ExtractIndex(uint64_t& start_index, int bits)
{
    uint64_t ind = 0ULL;
    for(int i=63;i>=0;i--) {
        uint64_t bit = 1ULL << (((i<<1)&0x3e)|(i>>5));
        ind = ind << 1;
        if (start_index&bit) {
            ind = ind | 1ULL;
        }
    }
    uint64_t mask = ((1ULL<<(bits))-1ULL);
    start_index = ind & mask;
    uint64_t white = kr02_whitening(start_index);
    bool valid_comp = (white<<bits)==(ind&(~mask)) ? 1 : 0;

#if 0
    if (valid_comp==0) {
        printf("%i, w:%08x m:%08x\n", (int)start_index, (int)(white>>bits),
               (int)(ind>>bits));
    }
#endif

    return valid_comp;
}

}
