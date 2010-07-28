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
#ifndef A5_CPU
#define A5_CPU

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


#include <queue>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include "Advance.h"
#include <map>
#include <deque>

using namespace std;

class DLL_LOCAL A5Cpu {
public:
  A5Cpu(int max_rounds, int condition, int threads);
  ~A5Cpu();

  int  Submit(uint64_t start_value, uint64_t target_value, int32_t start_round, 
              int32_t stop_round, uint32_t advance, void* context);

  bool PopResult(uint64_t& start_value, uint64_t& stop_value,
                 int32_t& start_round, void** context);

  static uint64_t ReverseBits(uint64_t r);
  static int PopcountNibble(int x);

private:
  void CalcTables(void);
  void Process(void);

  int mNumThreads;
  pthread_t* mThreads;
  static void* thread_stub(void* arg);
  static A5Cpu* mSpawner;

  unsigned int mCondition;
  unsigned int mMaxRound;

  bool mIsUsingTables;
  ushort mClockMask[16*16*16];
  unsigned char mTable6bit[1024];
  unsigned char mTable5bit[512];
  unsigned char mTable4bit[256];

  bool mRunning; /* false stops worker thread */

  /* Mutex semaphore to protect the queues */
  sem_t mMutex;
  deque<uint64_t> mInputStart;
  deque<uint64_t> mInputTarget;
  deque<int32_t>  mInputRound;
  deque<int32_t>  mInputRoundStop;
  deque<uint32_t> mInputAdvance;
  deque<void*>    mInputContext;
  /* OUtput queues */
  queue< pair<uint64_t,uint64_t> > mOutput;
  queue<int32_t>  mOutputStartRound;
  queue<void*>    mOutputContext;

  map< uint32_t, class Advance* > mAdvances;
};

#endif
