#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>


using namespace std;

static inline uint64_t getEnd(unsigned char* p)
{
    uint64_t e = 0;

    e =    (uint64_t)p[0]      | ((uint64_t)p[1]<<8)
        | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24)
        | ((uint64_t)p[4]<<32) | ((uint64_t)p[5]<<40);

    return e;
}

static inline uint64_t getIndex(unsigned char* p)
{
    uint64_t e = 0;

    e =    (uint64_t)p[6]      | ((uint64_t)p[7]<<8)
        | ((uint64_t)p[8]<<16) | ((uint64_t)p[9]<<24)
        | ((uint64_t)p[10]<<32);

    return e;
}



int main (int argc, char* argv[])
{
    char idx[16];

    unsigned char* block;
    block = new unsigned char[4096];
    unsigned int inBlock = 0;
    uint64_t indexOff = 0ULL;

    if (getuid()==0) {
        printf("ERROR: Running as root can have disasterous consequences if writing to the wrong block device. This program will not run as root. Please change the permissions on the block device you wish to operate on instead, as this is safer.\n");
        return 0;
    }

    if (argc<2) {
        printf("usage: %s sourcepath (device/dest)\n",argv[0]);
        return 0;
    }

    FILE* outf = NULL;
    if (argc>2) {
        outf = fopen(argv[2], "w");
        if (outf==NULL) {
            printf("Could not open %s for writing\n",argv[2]);
            return 0;
        }
    }

    string indexFile = string(argv[1])+string("/index.dat");
    FILE* idxf = fopen(indexFile.c_str(), "w");
    if (idxf==NULL) {
        printf("Could not open %s for writing\n",indexFile.c_str());
        if (outf) {
            fclose(outf);
        }
        return 0;
    }

    for(int part=0; part<256;part++) {
        snprintf(idx,16,"/%02x.tbl", part);
        string filename = string(argv[1])+string(idx);
        FILE* fd = fopen(filename.c_str(), "r");
        if (fd==NULL) {
            printf("Error, cant open %s for reading\n",argv[1]);
            fclose(idxf);
            if (outf) {
                fclose(outf);
            }
            return -1; 
        }

        fseek(fd ,0 ,SEEK_END );
        long size = ftell(fd);
        unsigned int num = size / 11;
        fseek(fd ,0 ,SEEK_SET );
        unsigned char* pData = new unsigned char[size];

        printf("Reading %s %i bytes.\n", filename.c_str(), (unsigned int)size);
        size_t read = fread(pData, 1, size, fd);
        assert(read==size);
        fclose(fd);

        size = size - (size%11);
        if (read!=size) {
            printf("Warning: file is not a multiple of 11 bytes long.\n");
        }

        unsigned char* pIn  = pData;
        for (unsigned int i=0; i<num; i++) {
            uint64_t p = getEnd(pIn)|(((uint64_t)part)<<48);
            if (indexOff==0ULL) {
                indexOff = p;
                uint64_t realOff = p << 8;
                fwrite(&realOff,sizeof(uint64_t),1,idxf);
            }
            uint64_t r = (p - indexOff)>>4;
            if (r>=0x40000000) {
                std::cout << "End point error: " << std::hex << r
                          << " " << indexOff << " " << p << "\n";
                printf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                       pIn[0], pIn[1], pIn[2], pIn[3], pIn[4], 
                       pIn[5], pIn[6], pIn[7], pIn[8], pIn[9], pIn[10]);

                assert(!"End points to sparse. Table may be too small.");
            }
            r = r | (getIndex(pIn)<<30);
            *(uint64_t*)(&block[inBlock*8]) = r;
            pIn += 11;
            inBlock++;

            if (inBlock>=512)
            {
                /* Write block */
                if (outf) {
                    assert(fwrite(block,1,4096,outf)==4096);
                }
                inBlock = 0;
                /* max value padding for last block */
                memset(block,255,4096);
                indexOff = 0ULL;
            }

        }

        delete [] pData;
    }

    if (inBlock)
    {
        /* Write last block */
        if (outf) {
            assert(fwrite(block,1,4096,outf)==4096);
        }
    }


    if (outf) {
        fclose(outf);
    }
    fclose(idxf);

    return 0;
}
