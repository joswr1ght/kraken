#define _FILE_OFFSET_BITS 64

#include "MultiFileWriter.h"
#include <stdio.h>
#include <string>
#include <assert.h>

using namespace std;


int main(int argc, char* argv[])
{
    if (argc<4) {
        printf("Explode compressed SSD image into sorted bucketfiles.\n");
        printf("usage: %s blockdev/file indexfile dest_path\n",argv[0]);
        return 0;
    }

    std::string outpath(argv[3]);
    MultiFileWriter mfwr(outpath);

    if (!mfwr.isOK()) {
        printf("ERROR opening multi file writer.\n");
        return -1;
    }

    /* Read Index file */
    FILE* fd = fopen(argv[2],"r");
    if (fd==NULL) {
        printf("ERROR: can't read %s.\n", argv[2]);
        return -1;
    }
    fseek(fd ,0 ,SEEK_END );
    size_t index_size = ftell(fd);
    fseek(fd ,0 ,SEEK_SET );
    size_t numblocks = index_size / sizeof(uint64_t);
    uint64_t* pIndex = new uint64_t[numblocks];
    size_t r = fread(pIndex,sizeof(uint64_t),numblocks,fd);
    assert(r==numblocks);
    fclose(fd);

    fd = fopen(argv[1],"r");
    if (fd==NULL) {
        printf("ERROR: can't read %s.\n", argv[1]);
        delete [] pIndex;
        return -1;
    }

    uint64_t* pData = new uint64_t[512];
    int simples = numblocks-1;
    int lastprog = 0;
    int div = simples/100;
    for (int i=0;i<simples;i++) {
        size_t r = fread(pData,sizeof(uint64_t),512,fd);
        uint64_t off = pIndex[i];
        for(int j=0;j<512;j++) {
            uint64_t v = pData[j];
            uint64_t index = v>>30;
            uint64_t endpoint = off+((v&0x3FFFFFFFULL)<<12);
            mfwr.Write(endpoint,index);
        }
        int prog = i/div;
        if (prog>lastprog) {
            printf("#");
            fflush(stdout);
            lastprog = prog;
        }
    }
    /* Last block */
    r = fread(pData,sizeof(uint64_t),512,fd);
    uint64_t off = pIndex[simples];
    for(int j=0;j<512;j++) {
        uint64_t v = pData[j];
        if (v==0xffffffffffffffffULL) break;
        uint64_t index = v>>30;
        uint64_t endpoint = off+((v&0x3FFFFFFFULL)<<12);
        mfwr.Write(endpoint,index);
    }

    fclose(fd);
    delete[] pIndex;
    delete[] pData;

    printf("\n");
    return 0;
}
