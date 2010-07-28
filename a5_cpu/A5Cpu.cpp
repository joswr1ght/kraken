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

#include "A5Cpu.h"
#include <stdio.h>
#include <sys/time.h>

using namespace std;

/**
 * Construct an instance of A5 Cpu searcher
 * create tables and streams
 */
A5Cpu::A5Cpu(int max_rounds, int condition, int threads)
{
  mCondition = 32 - condition;
  mMaxRound = max_rounds;

  /* Set up lookup tables */
  CalcTables();

  /* Init semaphore */
  sem_init( &mMutex, 0, 1 );

  /* Start worker thread */
  mRunning = true;
  mNumThreads = threads>16?16:threads;
  if (mNumThreads<1) mNumThreads=1;
  mThreads = new pthread_t[mNumThreads];
  for (int i=0; i<mNumThreads; i++) {
    pthread_create(&mThreads[i], NULL, thread_stub, (void*)this);
  }
}

void* A5Cpu::thread_stub(void* arg)
{
  if (arg) {
    A5Cpu* a5 = (A5Cpu*)arg;
    a5->Process();
  }
  return NULL;
}

/**
 * Destroy an instance of A5 Cpu searcher
 * delete tables and streams
 */
A5Cpu::~A5Cpu()
{
  /* stop worker thread */
  mRunning = false;
  for (int i=0; i<mNumThreads; i++) {
    pthread_join(mThreads[i], NULL);
  }

  delete [] mThreads;

  sem_destroy(&mMutex);
}
  
int  A5Cpu::Submit(uint64_t start_value, uint64_t target,
                   int32_t start_round, int32_t stop_round,
                   uint32_t advance, void* context)
{
  if (start_round>=mMaxRound) return -1;
  if (stop_round<0) stop_round = mMaxRound;

  int size = 0;
  sem_wait(&mMutex);
  if (target) {
      /* Keysearches are given priority */
      mInputStart.push_front(start_value);
      mInputTarget.push_front(target);
      mInputRound.push_front(start_round);
      mInputRoundStop.push_front(stop_round);
      mInputAdvance.push_front(advance);
      mInputContext.push_front(context);
  } else {
      mInputStart.push_back(start_value);
      mInputTarget.push_back(target);
      mInputRound.push_back(start_round);
      mInputRoundStop.push_back(stop_round);
      mInputAdvance.push_back(advance);
      mInputContext.push_back(context);
  }
  size = mInputRound.size();
  sem_post(&mMutex);
  return size;
}
  
bool A5Cpu::PopResult(uint64_t& start_value, uint64_t& stop_value,
                      int32_t& start_round, void** context)
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
    void* ctx = mOutputContext.front();
    mOutputContext.pop();
    if (context) *context = ctx;
  }
  sem_post(&mMutex);
  return res;
}

void A5Cpu::Process(void)
{
  bool active = false;
  struct timeval tStart;
  struct timeval tEnd;

  uint64_t start_point;
  uint64_t target;
  uint64_t start_point_r;
  int32_t  start_round;
  int32_t  stop_round;
  uint32_t advance;
  const uint32_t* RFtable;
  void* context;

  for(;;) {
    if (!mRunning) break;

    /* Get input */
    sem_wait(&mMutex);
    if (mInputStart.size()) {
      start_point = mInputStart.front();
      mInputStart.pop_front();
      target = mInputTarget.front();
      mInputTarget.pop_front();
      start_point_r = ReverseBits(start_point);
      start_round = mInputRound.front();
      mInputRound.pop_front();
      stop_round = mInputRoundStop.front();
      mInputRoundStop.pop_front();
      advance = mInputAdvance.front();
      mInputAdvance.pop_front();
      context =  mInputContext.front();
      mInputContext.pop_front();
      map< uint32_t, class Advance* >::iterator it = mAdvances.find(advance);
      if (it==mAdvances.end()) {
          class Advance* adv = new Advance(advance, mMaxRound);
          mAdvances[advance] = adv;
          RFtable = adv->getRFtable();
      } else {
          RFtable = (*it).second->getRFtable();
      }
      active = true;
      // printf("Insert\n");
    }
    sem_post(&mMutex);

    if (!active) {
      /* Don't use CPU while idle */
      usleep(250);
      continue;
    }

    gettimeofday( &tStart, NULL );
    /* Do something */
    unsigned int out_hi = start_point_r>>32;
    unsigned int out_lo = start_point_r;

    unsigned int target_lo = target;
    unsigned int target_hi = target >> 32;

    unsigned int last_key_lo;
    unsigned int last_key_hi;

    bool keysearch = (target != 0ULL);

    for (int round=start_round; round < stop_round; ) {
        out_lo = out_lo ^ RFtable[2*round];
        out_hi = out_hi ^ RFtable[2*round+1];

        if ((out_hi>>mCondition)==0) {
            // uint64_t res = (((uint64_t)out_hi)<<32)|out_lo;
            // res = ReverseBits(res);
            // printf("New round %i %016llx %08x:%08x\n", round, res, out_hi, out_lo);
            round++;
            if (round>=stop_round) break;
        }

        unsigned int lfsr1 = out_lo;
        unsigned int lfsr2 = (out_hi << 13) | (out_lo >> 19);
        unsigned int lfsr3 = out_hi >> 9;

        last_key_hi = out_hi;
        last_key_lo = out_lo;

        for (int i=0; i<25 ; i++) {
            int clocks = ((lfsr1<<3)&0xf00) | ((lfsr2>>3)&0xf0) | ((lfsr3>>7)&0xf);
            int masks = mClockMask[clocks];

            /* lfsr1 */
            unsigned int tmask = (masks>>8)&0x0f;
            unsigned int tval = mTable6bit[((lfsr1>>9)&0x3f0)|tmask];
            unsigned int tval2 = mTable4bit[((lfsr1>>6)&0xf0)|tmask];
            lfsr1 = (lfsr1<<(tval2>>4))^(tval>>4)^(tval2&0x0f);

            /* lfsr2 */
            tmask = (masks>>4)&0x0f;
            tval = mTable5bit[((lfsr2>>13)&0x1f0)|tmask];
            out_hi = out_hi ^ (tval&0x0f);
            lfsr2 = (lfsr2<<(masks>>12))^(tval>>4);

            /* lfsr3 */
            tmask = masks & 0x0f;
            tval = mTable6bit[((lfsr3>>13)&0x3f0)|tmask];
            tval2 = mTable4bit[(lfsr3&0xf0)|tmask];
            lfsr3 = (lfsr3<<(tval2>>4))^(tval>>4)^(tval2&0x0f);
        }
        for (int i=0; i<8 ; i++) {
            int clocks = ((lfsr1<<3)&0xf00) | ((lfsr2>>3)&0xf0) | ((lfsr3>>7)&0xf);
            int masks = mClockMask[clocks];
            
            /* lfsr1 */
            unsigned int tmask = (masks>>8)&0x0f;
            unsigned int tval = mTable6bit[((lfsr1>>9)&0x3f0)|tmask];
            out_hi = (out_hi << 4) | (tval&0x0f);
            unsigned int tval2 = mTable4bit[((lfsr1>>6)&0xf0)|tmask];
            lfsr1 = (lfsr1<<(tval2>>4))^(tval>>4)^(tval2&0x0f);

            /* lfsr2 */
            tmask = (masks>>4)&0x0f;
            tval = mTable5bit[((lfsr2>>13)&0x1f0)|tmask];
            out_hi = out_hi ^ (tval&0x0f);
            lfsr2 = (lfsr2<<(masks>>12))^(tval>>4);        

            /* lfsr3 */
            tmask = masks & 0x0f;
            tval = mTable6bit[((lfsr3>>13)&0x3f0)|tmask];
            out_hi =  out_hi ^ (tval&0x0f);
            tval2 = mTable4bit[(lfsr3&0xf0)|tmask];
            lfsr3 = (lfsr3<<(tval2>>4))^(tval>>4)^(tval2&0x0f);
        }
        for (int i=0; i<8 ; i++) {
            int clocks = ((lfsr1<<3)&0xf00) | ((lfsr2>>3)&0xf0) | ((lfsr3>>7)&0xf);
            int masks = mClockMask[clocks];

            /* lfsr1 */
            unsigned int tmask = (masks>>8)&0x0f;
            unsigned int tval = mTable6bit[((lfsr1>>9)&0x3f0)|tmask];
            out_lo = (out_lo << 4) | (tval&0x0f);
            unsigned int tval2 = mTable4bit[((lfsr1>>6)&0xf0)|tmask];
            lfsr1 = (lfsr1<<(tval2>>4))^(tval>>4)^(tval2&0x0f);

            /* lfsr2 */
            tmask = (masks>>4)&0x0f;
            tval = mTable5bit[((lfsr2>>13)&0x1f0)|tmask];
            out_lo = out_lo ^ (tval&0x0f);
            lfsr2 = (lfsr2<<(masks>>12))^(tval>>4);        

            /* lfsr3 */
            tmask = masks & 0x0f;
            tval = mTable6bit[((lfsr3>>13)&0x3f0)|tmask];
            out_lo =  out_lo ^ (tval&0x0f);
            tval2 = mTable4bit[(lfsr3&0xf0)|tmask];
            lfsr3 = (lfsr3<<(tval2>>4))^(tval>>4)^(tval2&0x0f);
        }
        if (keysearch&&(target_hi==out_hi)&&(target_lo==out_lo)) {
            /* report key as finishing state */
            out_hi = last_key_hi;
            out_lo = last_key_lo;
            start_round = -1;
            break;
        }
    }

    gettimeofday( &tEnd, NULL );
    unsigned int uSecs = 1000000 * (tEnd.tv_sec - tStart.tv_sec);
    uSecs += (tEnd.tv_usec - tStart.tv_usec);

    // printf("Completed in %i ms\n", uSecs/1000);

    /* Report completed chains */
    sem_wait(&mMutex);

    uint64_t res = (((uint64_t)out_hi)<<32)|out_lo;
    res = ReverseBits(res);
    mOutput.push( pair<uint64_t,uint64_t>(start_point,res) );
    mOutputStartRound.push( start_round );
    mOutputContext.push( context );
    active = false;

    sem_post(&mMutex);

  }
}

/* Reverse bit order of an unsigned 64 bits int */
uint64_t A5Cpu::ReverseBits(uint64_t r)
{
  uint64_t r1 = r;
  uint64_t r2 = 0;
  for (int j = 0; j < 64 ; j++ ) {
    r2 = (r2<<1) | (r1 & 0x01);
    r1 = r1 >> 1;
  }
  return r2;
}

int A5Cpu::PopcountNibble(int x) {
    int res = 0;
    for (int i=0; i<4; i++) {
        res += x & 0x01;
        x = x >> 1;
    }
    return res;
}


void A5Cpu::CalcTables(void)
{
   /* Calculate clocking table */
    for(int i=0; i< 16 ; i++) {
        for(int j=0; j< 16 ; j++) {
            for(int k=0; k< 16 ; k++) {
                /* Copy input */
                int m1 = i;
                int m2 = j;
                int m3 = k;
                /* Generate masks */
                int cm1 = 0;
                int cm2 = 0;
                int cm3 = 0;
		/* Counter R2 */
		int r2count = 0;
                for (int l = 0; l < 4 ; l++ ) {
                    cm1 = cm1 << 1;
                    cm2 = cm2 << 1;
                    cm3 = cm3 << 1;
                    int maj = ((m1>>3)+(m2>>3)+(m3>>3))>>1;
                    if ((m1>>3)==maj) {
                        m1 = (m1<<1)&0x0f;
                        cm1 |= 0x01;
                    }
                    if ((m2>>3)==maj) {
                        m2 = (m2<<1)&0x0f;
                        cm2 |= 0x01;
			r2count++;
                    }
                    if ((m3>>3)==maj) {
                        m3 = (m3<<1)&0x0f;
                        cm3 |= 0x01;
                    }
                }
                // printf( "%x %x %x -> %x:%x:%x\n", i,j,k, cm1, cm2, cm3);
                int index = i*16*16+j*16+k;
                mClockMask[index] = (r2count<<12) | (cm1<<8) | (cm2<<4) | cm3;
            }
        }
    }

    /* Calculate 111000 + clock mask table */
    for (int i=0; i < 64 ; i++ ) {
        for(int j=0; j<16; j++) {
            int count = PopcountNibble(j);
            int feedback = 0;
            int data = i;
            for (int k=0; k<count; k++) {
                feedback = feedback << 1;
                int v = (data>>5) ^ (data>>4) ^ (data>>3);
                data = data << 1;
                feedback ^= (v&0x01);
            }
            data = i;
            int mask = j;
            int output = 0;
            for (int k=0; k<4; k++) {
                output = (output<<1) ^ ((data>>5)&0x01);
                if (mask&0x08) {
                    data = data << 1;
                }
                mask = mask << 1;
            }
            int index = i * 16 + j; 
            mTable6bit[index] = (feedback<<4) | output;
            // printf("%02x:%x -> %x %x\n", i,j,feedback, output);
        }
    }

    /* Calculate 11000 + clock mask table */
    for (int i=0; i < 32 ; i++ ) {
        for(int j=0; j<16; j++) {
            int count = PopcountNibble(j);
            int feedback = 0;
            int data = i;
            for (int k=0; k<count; k++) {
                feedback = feedback << 1;
                int v = (data>>4) ^ (data>>3);
                data = data << 1;
                feedback ^= (v&0x01);
            }
            data = i;
            int mask = j;
            int output = 0;
            for (int k=0; k<4; k++) {
                output = (output<<1) ^ ((data>>4)&0x01);
                if (mask&0x08) {
                    data = data << 1;
                }
                mask = mask << 1;
            }
            int index = i * 16 + j; 
            mTable5bit[index] = (feedback<<4) | output;
            // printf("%02x:%x -> %x %x\n", i,j,feedback, output);
        }
    }

    /* Calculate 1000 + clock mask table */
    for (int i=0; i < 16 ; i++ ) {
        for(int j=0; j<16; j++) {
            int count = PopcountNibble(j);
            int feedback = 0;
            int data = i;
            for (int k=0; k<count; k++) {
                feedback = feedback << 1;
                int v = (data>>3);
                data = data << 1;
                feedback ^= (v&0x01);
            }
            int index = i * 16 + j;
            mTable4bit[index] = (count<<4)|feedback;
            // printf("%02x:%x -> %x\n", i,j,feedback );
        }
    }
}


/* Stubs for shared library - exported without name mangling */

extern "C" {

static class A5Cpu* a5Instance = 0;

bool DLL_PUBLIC A5CpuInit(int max_rounds, int condition, int threads)
{
  if (a5Instance) return false;
  a5Instance = new A5Cpu(max_rounds, condition, threads );
  return true;
}

int  DLL_PUBLIC A5CpuSubmit(uint64_t start_value, int32_t start_round,
                            uint32_t advance, void* context)
{
  if (a5Instance) {
      return a5Instance->Submit(start_value, 0ULL, start_round, -1, advance, context);
  }
  return -1; /* Error */
}

int  DLL_PUBLIC A5CpuKeySearch(uint64_t start_value, uint64_t target, int32_t start_round,
                               int32_t stop_round, uint32_t advance, void* context)
{
  if (a5Instance) {
      return a5Instance->Submit(start_value, target, start_round, stop_round, advance, context);
  }
  return -1; /* Error */
}


bool DLL_PUBLIC A5CpuPopResult(uint64_t& start_value, uint64_t& stop_value,
                               int32_t& start_round, void** context)
{
  if (a5Instance) {
      return a5Instance->PopResult(start_value, stop_value, start_round, context);
  }
  return false; /* Nothing popped */ 
}

void DLL_PUBLIC A5CpuShutdown()
{
  delete a5Instance;
  a5Instance = NULL;
}

}
