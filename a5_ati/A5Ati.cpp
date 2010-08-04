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

#include "A5Ati.h"
#include <stdio.h>
#include <sys/time.h>
#include "A5Slice.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;


/**
 * Construct an instance of A5 Ati searcher
 * create tables and streams
 */
AtiA5::AtiA5(int max_rounds, int condition, unsigned int gpu_mask, int pipeline_mul)
{
    mCondition = condition;
    mMaxRound = max_rounds;
    mPipelineSize = 0;
    mPipelineMul = pipeline_mul;
    mGpuMask = gpu_mask;

    /* Init semaphore */
    sem_init( &mMutex, 0, 1 );

    /* Start worker thread */
    mRunning = true;
    pthread_create(&mThread, NULL, thread_stub, (void*)this);
}

void* AtiA5::thread_stub(void* arg)
{
    if (arg) {
        AtiA5* a5 = (AtiA5*)arg;
        a5->Process();
    }
    return NULL;
}

/**
 * Destroy an instance of A5 Ati searcher
 * delete tables and streams
 */
AtiA5::~AtiA5()
{
    /* stop worker thread */
    mRunning = false;
    pthread_join(mThread, NULL);
    
    sem_destroy(&mMutex);
}
  
int  AtiA5::Submit(uint64_t start_value, unsigned int start_round, uint32_t advance, void* context)
{
    if (start_round>=mMaxRound) return -1;

    int size = 0;
    sem_wait(&mMutex);
    mInputStart.push_back(start_value);
    mInputRoundStart.push_back(start_round);
    mInputRoundStop.push_back(mMaxRound);
    mInputAdvance.push_back(advance);
    mInputContext.push_back(context);
    size = mInputRoundStart.size();
    sem_post(&mMutex);
    return size;
}

int  AtiA5::SubmitPartial(uint64_t start_value, unsigned int stop_round, uint32_t advance, void* context)
{
    if (stop_round>mMaxRound) return -1;

    int size = 0;
    sem_wait(&mMutex);
    mInputStart.push_front(start_value);
    mInputRoundStart.push_front(0);
    mInputRoundStop.push_front(stop_round);
    mInputAdvance.push_front(advance);
    mInputContext.push_front(context);
    size = mInputRoundStart.size();
    sem_post(&mMutex);
    return size;
}
  
bool AtiA5::PopResult(uint64_t& start_value,
			uint64_t& stop_value, void** context)
{
    bool res = false;
    sem_wait(&mMutex);
    if (mOutput.size()>0) {
        res = true;
        pair<uint64_t,uint64_t> res = mOutput.front();
        mOutput.pop();
        start_value = res.first;
        stop_value = res.second;
        void* ctx = mOutputContext.front();
        mOutputContext.pop();
        if (context) *context = ctx;
    }
    sem_post(&mMutex);
    return res;
}

bool AtiA5::PopRequest(JobPiece_s* job)
{
    bool res = false;
    sem_wait(&mMutex);
    if (mInputStart.size()>0) {
        res = true;
        job->start_value = mInputStart.front();
        mInputStart.pop_front();
        job->start_round = mInputRoundStart.front();
        mInputRoundStart.pop_front();
        job->end_round = mInputRoundStop.front()-1;
        mInputRoundStop.pop_front();
        job->current_round = job->start_round;
        job->context = mInputContext.front();
        mInputContext.pop_front();
        unsigned int advance = mInputAdvance.front();
        mInputAdvance.pop_front();

        Advance* pAdv;
        map<uint32_t,Advance*>::iterator it = mAdvanceMap.find(advance);
        if (it==mAdvanceMap.end()) {
            pAdv = new Advance(advance,mMaxRound);
            mAdvanceMap[advance]=pAdv;
        } else {
            pAdv = (*it).second;
        }
        job->round_func = pAdv->getAdvances();
        job->cycles = 0;
        job->idle = false;
    }
    sem_post(&mMutex);
    return res;
}

void AtiA5::PushResult(JobPiece_s* job)
{
    sem_wait(&mMutex);
    mOutput.push( pair<uint64_t,uint64_t>(job->start_value,job->end_value) );
    mOutputContext.push(job->context);
    sem_post(&mMutex);
}

bool AtiA5::PipelineInfo(int &length)
{
    printf("Query pipesize: %i\n", mPipelineSize);
    if (mPipelineSize<=0) {
        return false;
    }
    length = mPipelineSize;
    return true;
}

void AtiA5::Process(void)
{
    struct timeval tStart;
    struct timeval tEnd;

    int numCores = A5Slice::getNumDevices();
    int numSlices = 0;
    for(int i=0; i<numCores; i++) {
        if ((1<<i)&mGpuMask) numSlices++;
    }
    printf("Running on %i GPUs\n", numSlices);
    
    int pipes = 0;

    A5Slice* slices[numSlices];
    
    int core = 0;
    for( int i=0; i<numCores ; i++ ) {
        if ((1<<i)&mGpuMask) {
            slices[core] = new A5Slice( this, i, mCondition,
                                        mMaxRound, mPipelineMul );
            pipes += 32*slices[core]->getNumSlots();
            core++;
        }
    }

    /* update after everything is created, compiled and linked */
    mPipelineSize = pipes;    

    for(;;) {
        bool newCmd = false;

        sem_wait(&mMutex);
        int available = mInputStart.size();
        sem_post(&mMutex);

        int total = available;
        for( int i=0; i<numSlices ; i++ ) {
            total += slices[i]->getNumJobs();
        }        

        if (total) {
            for( int i=0; i<numSlices ; i++ ) {
                slices[i]->makeAvailable(available);
                newCmd |= slices[i]->tick();
            }
        } else {
            /* Empty pipeline */
            usleep(1000);
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
uint64_t AtiA5::ReverseBits(uint64_t r)
{
    uint64_t r1 = r;
    uint64_t r2 = 0;
    for (int j = 0; j < 64 ; j++ ) {
        r2 = (r2<<1) | (r1 & 0x01);
        r1 = r1 >> 1;
    }
    return r2;
}

/* Stubs for shared library - exported without name mangling */

extern "C" {

static class AtiA5* a5Instance = 0;

bool DLL_PUBLIC A5AtiInit(int max_rounds, int condition, unsigned int gpu_mask, int pipeline_mul)
{
    if (a5Instance) return false;
    a5Instance = new AtiA5(max_rounds, condition, gpu_mask, pipeline_mul);
    return true;
}

bool DLL_PUBLIC A5AtiPipelineInfo(int &length)
{
    if (a5Instance) {
        return a5Instance->PipelineInfo(length);
    }
    return false;
}

int  DLL_PUBLIC A5AtiSubmit(uint64_t start_value, unsigned int start_round, uint32_t advance, void* context)
{
    if (a5Instance) {
        return a5Instance->Submit(start_value, start_round, advance, context);
    }
    return -1; /* Error */
}

int  DLL_PUBLIC A5AtiSubmitPartial(uint64_t start_value, unsigned int stop_round, uint32_t advance, void* context)
{
    if (a5Instance) {
        return a5Instance->SubmitPartial(start_value, stop_round, advance, context);
    }
    return -1; /* Error */
}


bool DLL_PUBLIC A5AtiPopResult(uint64_t& start_value, uint64_t& stop_value, void** context)
{
    if (a5Instance) {
        return a5Instance->PopResult(start_value, stop_value, context);
    }
    return false; /* Nothing popped */ 
}

void DLL_PUBLIC A5AtiShutdown()
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
