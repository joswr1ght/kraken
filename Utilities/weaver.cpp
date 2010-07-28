#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <sys/stat.h>

using namespace std;

/*********************************************
 *
 * Weave together 2 sorted tables.
 * Output as combined sorted table,
 * with merges removed as usual.
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

size_t ReadFile(string &name, size_t* alloc_size, unsigned char** ppData)
{
    FILE* fd = fopen(name.c_str(), "r");

    if (fd==NULL) {
        fprintf(stderr, "Error, cant open %s for reading\n",name.c_str());
        assert(0);
    }

    fseek(fd ,0 ,SEEK_END );
    size_t size = ftell(fd);
    fseek(fd ,0 ,SEEK_SET );

    if (size>*alloc_size) {
        *alloc_size = size + size / 5;
        delete [] *ppData;
        *ppData = new unsigned char[*alloc_size];
    }
 
    size_t r = fread(*ppData, 1, size, fd);
    assert(r==size);
    fclose(fd);
    return size;
}


int main (int argc, char* argv[])
{
    size_t d1size = 0;
    size_t d2size = 0;
    size_t d3size = 0;
    unsigned char* pData1 = NULL;
    unsigned char* pData2 = NULL;
    unsigned char* pData3 = NULL;
    char idx[16];

    if (argc<4) {
        printf("usage: %s source1 source2 dest\n",argv[0]);
        return 0;
    }

    struct stat st;
    if(stat(argv[3],&st) != 0) {
        printf("%s does not exist, attempting creation.\n",argv[3]);
        mkdir(argv[3],S_IRUSR|S_IWUSR|S_IXUSR);
    }

    for (int num=0; num<256; num++) {
        snprintf(idx,16,"/%02x.tbl",num);
        string filename = string(argv[1])+string(idx);
        printf("Reading: %s\n", filename.c_str());
        size_t sz1 = ReadFile(filename,&d1size,&pData1);
        assert((sz1%11)==0);
        filename = string(argv[2])+string(idx);
        printf("Reading: %s\n", filename.c_str());
        size_t sz2 = ReadFile(filename,&d2size,&pData2);
        assert((sz2%11)==0);
        size_t sz3=sz1+sz2;
        if (sz3>d3size) {
            d3size = sz3 + sz3 / 5;
            delete [] pData3;
            pData3 = new unsigned char[d3size];
        }

        size_t num1 = sz1 / 11;
        size_t num2 = sz2 / 11;
        size_t num3 = 0;

        unsigned char* pIn1 = pData1;
        unsigned char* pIn2 = pData2;
        unsigned char* pOut = pData3;
        while (num1&&num2) {
            if (compare(pIn1,pIn2)>0) {
                memcpy(pOut,pIn2,11);
                pIn2 += 11;
                num2--;
            } else {
                memcpy(pOut,pIn1,11);
                pIn1 += 11;
                num1--;
            }
            pOut+=11;
            num3++;
        }
        /* Only 1 left */
        if (num1) {
            memcpy(pOut,pIn1,11);
            num3++;            
        } else if (num2) {
            memcpy(pOut,pIn2,11);
            num3++;            
        }

        /* Compacting, eliminating merges */
        pOut = pData3;
        unsigned char* pIn  = pData3;
        unsigned char* pLast  = NULL;
        for (unsigned int i=0; i < num3; i++) {
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

        printf("%i total, %i without merges\n",(int)num3,(int)(pOut-pData3)/11);
        filename = string(argv[3])+string(idx);
        printf("Writing: %s\n", filename.c_str());

        FILE* fd = fopen(filename.c_str(), "r");
        if (fd!=NULL) {
            fprintf(stderr,"ERROR: %s exists. Will not overwrite, please delete manually.\n", filename.c_str());
            assert(0);
        }

        fd = fopen(filename.c_str(), "w");
        if (fd) {
            size_t num_out = pOut-pData3;
            size_t num_written = fwrite(pData3, 1, num_out, fd);
            fclose(fd);
            assert( num_out==num_written );
        } else {
            fprintf(stderr,"Can't write to %s\n", filename.c_str());
            assert(0);
        }
    }

    delete [] pData1;
    delete [] pData2;
    delete [] pData3;

    return 0;
}
