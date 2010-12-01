#ifndef NCQ_DEVICE_H
#define NCQ_DEVICE_H

#include <stdint.h>
#include <queue>
#include <semaphore.h>
#if defined(__APPLE__)
#define mmap64          mmap
#endif /* __APPLE__ */

using namespace std;

class NcqDevice;

class NcqRequestor {
public:
    NcqRequestor() {};
    virtual ~NcqRequestor() {};

private:
    friend class NcqDevice;
    virtual void processBlock(const void* pDataBlock) = 0;
};

class NcqDevice {
public:
    NcqDevice(const char* pzDevNode);
    ~NcqDevice();

    void Request(class NcqRequestor*, uint64_t blockno);
    void Cancel(class NcqRequestor*);

    typedef struct {
        class NcqRequestor* req;
        uint64_t            blockno;
    } request_t;

    typedef struct {
        class NcqRequestor* req;
        uint64_t            blockno;
        void*               addr;
        int                 next_free;
    } mapRequest_t;

    static void* thread_stub(void* arg);
    void WorkerThread();

private:
    int mDevice;
    unsigned char mBuffer[4096];
    mapRequest_t mMappings[32];
    queue< request_t > mRequests;
    int mFreeMap;
    sem_t mMutex;
    pthread_t mWorker;
    bool mRunning;
    char mDevC;
};


#endif
