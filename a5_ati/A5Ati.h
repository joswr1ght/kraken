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

#ifndef A5_ATI
#define A5_ATI

/* DLL export incatantion */
#if defined _WIN32 || defined __CYGWIN__
#  ifdef BUILDING_DLL
#    ifdef __GNUC__
#      define DLL_PUBLIC __attribute__((dllexport))
#    else
#      define DLL_PUBLIC __declspec(dllexport)
#    endif
#  else
#    ifdef __GNUC__
#      define DLL_PUBLIC __attribute__((dllimport))
#    else
#      define DLL_PUBLIC __declspec(dllimport)
#    endif
#    define DLL_LOCAL
#  endif
#else
#  if __GNUC__ >= 4
#    define DLL_PUBLIC __attribute__ ((visibility("default")))
#    define DLL_LOCAL  __attribute__ ((visibility("hidden")))
#  else
#    define DLL_PUBLIC
#    define DLL_LOCAL
#  endif
#endif


#include <stdint.h>
#include <queue>
#include <deque>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include "Advance.h"
#include <map>

using namespace std;

class A5Slice;

class DLL_LOCAL AtiA5 {
public:
    AtiA5(int max_rounds, int condition, unsigned int gpu_mask, int pipeline_mul);
    ~AtiA5();
    bool PipelineInfo(int &length);
    int  Submit(uint64_t start_value, unsigned int start_round, uint32_t advance, void* context);
    int  SubmitPartial(uint64_t start_value, unsigned int stop_round, uint32_t advance, void* context);
    bool PopResult(uint64_t& start_value, uint64_t& stop_value, void** context);

    static uint64_t ReverseBits(uint64_t r);
    static uint64_t AdvanceRFlfsr(uint64_t v);

    typedef struct {
        uint64_t start_value;
        uint64_t end_value;
        int start_round;
        int end_round;
        const uint64_t* round_func;
        void* context;
        int next_free;  /* for free list housekeeping */
        unsigned int current_round;
        unsigned int cycles;
        bool idle;
    } JobPiece_s; 

private:
    friend class A5Slice;

    bool PopRequest(JobPiece_s*);
    void PushResult(JobPiece_s*);

    void Process(void);

    pthread_t mThread;
    static void* thread_stub(void* arg);
    static AtiA5* mSpawner;

    unsigned int mCondition;
    unsigned int mMaxRound;
    int mPipelineSize;
    int mPipelineMul;

    bool mRunning; /* false stops worker thread */
    unsigned int mGpuMask;

    /* Mutex semaphore to protect the queues */
    sem_t mMutex;
    /* Input queues */
    deque<uint64_t> mInputStart;
    deque<int>      mInputRoundStart;
    deque<int>      mInputRoundStop;
    deque<void*>    mInputContext;
    deque<uint32_t> mInputAdvance;
    /* Output queues */
    queue< pair<uint64_t,uint64_t> > mOutput;
    queue<void*>    mOutputContext;
    /* Mutex protected advance map */
    map< uint32_t, class Advance* > mAdvanceMap;
};

#endif
