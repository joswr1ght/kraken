#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <map>
#include <iostream>
#include <unistd.h>

#include "../a5_cpu/A5CpuStubs.h"

using namespace std;

static inline uint64_t getIndex(unsigned char* p)
{
    uint64_t e = 0;

    e =    (uint64_t)p[6]      | ((uint64_t)p[7]<<8)
        | ((uint64_t)p[8]<<16) | ((uint64_t)p[9]<<24)
        | ((uint64_t)p[10]<<32);

    return e;
}

inline uint64_t getEnd(unsigned char* p)
{
    uint64_t e = 0;

    e =    (uint64_t)p[0]      | ((uint64_t)p[1]<<8)
        | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24)
        | ((uint64_t)p[4]<<32) | ((uint64_t)p[5]<<40);

    return e;
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

void ApplyIndexFunc(uint64_t& start_index, int bits)
{
    uint64_t w = kr02_whitening(start_index);
    start_index = kr02_mergebits((w<<bits)|start_index);
}

int ExtractIndex(uint64_t& start_index, int bits)
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

    return valid_comp;
}


/*********************************************
 *
 * Check a single table file for consistency
 *
 * Author: Frank A. Stevenson
 * Copyright 2010
 *
 ********************************************/

std::map<uint64_t,uint64_t> lookups;

bool CheckResult()
{
    uint64_t result;
    uint64_t start_val;
    uint64_t stop_val;
    int start_rnd;
    std::map<uint64_t,uint64_t>::iterator it;

    while (A5CpuPopResult(start_val, stop_val, start_rnd, NULL)) {
        it = lookups.find(start_val);
        if (it==lookups.end()) {
            fprintf(stderr,"ERROR: unknown chain encounterd. (Error in program).\n");
        } else {
            uint64_t table_end = it->second;
            if ((stop_val&0x00ffffffffffff00ULL)!=table_end) {
                fprintf(stderr,"ERROR: Error in chain/table.\n");
                uint64_t index = start_val;
                ExtractIndex(index,34);
                fprintf(stderr,"%016llx -> %016llx should be ??%014llx from index %010llx\n",start_val,stop_val,table_end,index);
            }
            lookups.erase(it);
        }
        
    }
    return (lookups.size()>0);
}


int main (int argc, char* argv[])
{
    if (argc<3) {
        printf("usage: %s fileXX.tbl id (top)\n",argv[0]);
        return 0;
    }

    FILE* fd = fopen(argv[1], "r");

    if (fd==NULL) {
        fprintf(stderr, "Error, cant open %s for reading\n",argv[1]);
        return -1;
    }

    fseek(fd ,0 ,SEEK_END );
    size_t size = ftell(fd);
    fseek(fd ,0 ,SEEK_SET );

    if (size==0)
    {
        fprintf(stderr,"ERROR: file is 0 bytes.\n");
        return -1;
    }

    unsigned char* pData = new unsigned char[size];
    size_t r = fread(pData, 1, size, fd);
    assert(r==size);
    fclose(fd);

    int id = atoi(argv[2]);
    printf("Table id is %i\n", id );
    A5CpuInit(8, 12, 2);
    int checked = 0;

    size_t num = size / 11;
    uint64_t low = getIndex(pData);
    uint64_t high = low;

    unsigned char* pC = pData;

    size_t tocheck = 0;
    size_t interval = 1000000;

    if ((argc>3)&&(strcmp(argv[3],"top")==0)) {
        interval = 10000;
        tocheck = num - (num/20);
        printf("Only checking top.\n");
    }

    for(size_t i=0;i<num;i++)
    {
        uint64_t index = getIndex(pC);

        if ((i>tocheck)&&(i%interval==0)) {
            /* check this one */
            uint64_t search_start = index;
            ApplyIndexFunc(search_start, 34);
            A5CpuSubmit(search_start, 0, id, NULL);
            uint64_t end = getEnd(pC)<<8;
            lookups[search_start] = end;
            checked++;
        }

        if (low>index) low = index;
        else if (high<index) high = index;
        pC+=11;
    }

    delete[] pData;

    cout << "Low: " << low << "\n";
    cout << "High: " << high << "\n";

    while (CheckResult())
    {
        usleep(200);
    }
    printf("Checked: %i\n",checked);


    return 0;
}
