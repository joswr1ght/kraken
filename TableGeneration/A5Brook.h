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

#ifndef A5_BROOK
#define A5_BROOK

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
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;

class A5Slice;

class DLL_LOCAL BrookA5 {
public:
  BrookA5(int advance, int max_rounds, int condition, int numcores);
  ~BrookA5();
  bool PipelineInfo(int &length);
  int  Submit(uint64_t start_value, unsigned int start_round);
  int  SubmitPartial(uint64_t start_value, unsigned int stop_round);
  bool PopResult(uint64_t& start_value, uint64_t& stop_value, int& start_round);

  static uint64_t ReverseBits(uint64_t r);
  static uint64_t AdvanceRFlfsr(uint64_t v);

private:
friend class A5Slice;

  bool PopRequest(uint64_t& start_value, unsigned int& start_round);
  void PushResult(uint64_t start_value, uint64_t stop_value, int start_round);

  void Process(void);

  pthread_t mThread;
  static void* thread_stub(void* arg);
  static BrookA5* mSpawner;

  unsigned int mCondition;
  unsigned int mMaxRound;
  unsigned int mAdvance;
  int mNumCores;
  int mPiplineSize;

  unsigned int* mRFtable;

  bool mRunning; /* false stops worker thread */

  /* Mutex semaphore to protect the queues */
  sem_t mMutex;
  queue<uint64_t> mInputStart;
  queue<int>      mInputRound;
  queue< pair<uint64_t,uint64_t> > mOutput;
  queue<int> mOutputStartRound;
};

#endif
