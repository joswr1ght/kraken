#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>

using namespace std;

/*********************************************
 *
 * Batch sorter for all individual files
 * in a Multi File Table of chains.
 *
 * Spawns multiple threads that serialize
 * IO operations to avoid disk trashing
 *
 * Author: Frank A. Stevenson
 *
 * Copyright 2010
 *
 ********************************************/

int compare(const void * a, const void * b)
{
    unsigned char* pA = (unsigned char*)a;
    unsigned char* pB = (unsigned char*)b;

    int v = pA[5] - pB[5];
    if (v) return v;
    v = pA[4] - pB[4];
    if (v) return v;
    v = pA[3] - pB[3];
    if (v) return v;
    v = pA[2] - pB[2];
    if (v) return v;
    v = pA[1] - pB[1];
    if (v) return v;
    v = pA[0] - pB[0];
    if (v) return v;
    /* end points equal, compare index next */
    v = pA[10] - pB[10];
    if (v) return v;
    v = pA[9] - pB[9];
    if (v) return v;
    v = pA[8] - pB[8];
    if (v) return v;
    v = pA[7] - pB[7];
    if (v) return v;
    v = pA[6] - pB[6];
    return v;
}

static string gSourcePath;
static string gDestPath;
static int gCurrent = 0;
static sem_t gReadMutex;
static sem_t gWriteMutex;

static void* sort_worker(void *arg)
{
    char idx[16];
    unsigned char* pData = NULL;
    size_t pDataSize = 0;

    for(;;) {
        sem_wait(&gReadMutex);
        if (gCurrent>255) {
            sem_post(&gReadMutex);
            break;
        }
        snprintf(idx,16,"/%02x.tbl",gCurrent);
        gCurrent++;

        string filename = gSourcePath + string(idx);
        printf("Reading %s\n",filename.c_str());

        FILE* fd = fopen(filename.c_str(), "r");
        if (fd==NULL) {
            printf("Error, cant open %s for reading\n",filename.c_str());
            sem_post(&gReadMutex);
            continue; 
        }
    
        fseek(fd ,0 ,SEEK_END );
        long size = ftell(fd);
        unsigned int num = size / 11;
        fseek(fd ,0 ,SEEK_SET );
        if (pDataSize < size) {
            /* Give 20% extra headroom to prevent frequent realloc
             * and fragmentation.
             */
            pDataSize = size + size / 5;
            delete [] pData;
            pData = new unsigned char[pDataSize];
        }
        size_t r = fread(pData, 1, size, fd);
        sem_post(&gReadMutex);
        assert(r==size);
        fclose(fd);

        size = size - (size%11);
        if (r!=size) {
            printf("Warning: file %s is not a multiple of 11 bytes long.\n",
                   filename.c_str());
        }

        qsort(pData, num, 11, compare);

        /* Compacting, eliminating merges */
        unsigned char* pOut = pData;
        unsigned char* pIn  = pData;
        unsigned char* pLast  = NULL;
        for (unsigned int i=0; i < num; i++) {
            if (pLast && (memcmp(pIn,pLast,6)==0)) {
                pIn+=11;
                continue;
            }
            if(pIn!=pOut) {
                memcpy(pOut,pIn,11);
            }
            pLast = pOut;
            pIn+=11;
            pOut+=11;
        }

        sem_wait(&gWriteMutex);
        filename = gDestPath + string(idx);
        fd = fopen(filename.c_str(), "r");
        if (fd!=NULL) {
            fprintf(stderr,"ERROR: %s exists. Will not overwrite, please delete manually.\n", filename.c_str());
            assert(0);
        }

        fd = fopen(filename.c_str(), "w");
        if (fd) {
            fwrite(pData, 1, (pOut-pData), fd);
            fclose(fd);
        } else {
            fprintf(stderr,"Can't write to %s\n", filename.c_str());
            assert(0);
        }
        sem_post(&gWriteMutex);

    }
    delete [] pData;

}

#define THREADS 3

int main (int argc, char* argv[])
{
    if (argc<3) {
        printf("usage: %s sourcepath destpath\n",argv[0]);
        return 0;
    }

    pthread_t threads[THREADS];

    sem_init(&gReadMutex,0,1);
    sem_init(&gWriteMutex,0,1);
    
    gSourcePath = string(argv[1]);
    gDestPath = string(argv[2]);

    struct stat st;
    if(stat(argv[2],&st) != 0) {
        printf("%s does not exist, attempting creation.\n",argv[2]);
        mkdir(argv[2],S_IRUSR|S_IWUSR|S_IXUSR);
    }

    for(int i=0; i< THREADS; i++) {
        pthread_create(&threads[i], NULL, sort_worker, NULL);
    }

    for(int i=0; i< THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&gReadMutex);
    sem_destroy(&gWriteMutex);

    return 0;
}
