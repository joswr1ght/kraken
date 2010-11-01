#define _FILE_OFFSET_BITS 64

#include "NcqDevice.h"
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

NcqDevice::NcqDevice(const char* pzDevNode)
{
#ifdef O_BINARY
    mDevice = open(pzDevNode, O_RDONLY | O_BINARY);
#else
    mDevice = open(pzDevNode, O_RDONLY);
#endif
    if (mDevice < 0) { perror(pzDevNode); exit(1); }

    /* Set up free list */
    for (int i=0; i<32; i++) {
        mMappings[i].addr = NULL;  /* idle */
        mMappings[i].next_free = i - 1;
    }
    mFreeMap = 31;

    /* Init semaphore */
    sem_init( &mMutex, 0, 1 );

    /* Start worker thread */
    mRunning = true;
    mDevC = pzDevNode[7]; // Drive letter
    pthread_create(&mWorker, NULL, thread_stub, (void*)this);
}

    
NcqDevice::~NcqDevice()
{
    mRunning = false;
    pthread_join(mWorker, NULL);
    close(mDevice);
    sem_destroy(&mMutex);
}

void* NcqDevice::thread_stub(void* arg)
{
    if (arg) {
        NcqDevice* nd = (NcqDevice*)arg;
        nd->WorkerThread();
    }
    return NULL;
}
 
void NcqDevice::Request(class NcqRequestor* req, uint64_t blockno)
{
#if 0
    lseek(mDevice, blockno*4096, SEEK_SET );
    size_t r = read(mDevice,mBuffer,4096);
    assert(r==4096);
    req->processBlock(mBuffer);
#else
    sem_wait(&mMutex);
    request_t qr;
    qr.req = req;
    qr.blockno = blockno;
    mRequests.push(qr);
    sem_post(&mMutex);
#endif
}

void NcqDevice::Cancel(class NcqRequestor*)
{
}

void NcqDevice::WorkerThread()
{
    unsigned char core[4];

    while (mRunning) {
        usleep(500);
        sem_wait(&mMutex);
        /* schedule requests */
        while ((mFreeMap>=0)&&(mRequests.size()>0))
        {
            int free = mFreeMap;
            mFreeMap = mMappings[free].next_free;
            request_t qr = mRequests.front();
            mRequests.pop();
            mMappings[free].req = qr.req;
            mMappings[free].blockno = qr.blockno;
            mMappings[free].addr = mmap64(NULL, 4096, PROT_READ,
                                          MAP_PRIVATE|MAP_FILE, mDevice,
                                          qr.blockno*4096);
            // printf("Mapped %p %lli\n", mMappings[free].addr, qr.blockno);
            madvise(mMappings[free].addr,4096,MADV_WILLNEED);
        }
        sem_post(&mMutex);

        /* Check request */
        for (int i=0; i<32; i++) {
            if (mMappings[i].addr) {
                mincore(mMappings[i].addr,4096,core);
                if (core[0]&0x01) {
                    // Debug disk access
                    // printf("%c",mDevC);
                    // fflush(stdout);
                    /* mapped & ready for use */
                    mMappings[i].req->processBlock(mMappings[i].addr);
                    munmap(mMappings[i].addr,4096);
                    mMappings[i].addr = NULL;
                    /* Add to free list */
                    sem_wait(&mMutex);
                    mMappings[i].next_free = mFreeMap;
                    mFreeMap = i;
                    sem_post(&mMutex);
                } else {
                    madvise(mMappings[i].addr,4096,MADV_WILLNEED);
                }
            }
        }
    }
}



