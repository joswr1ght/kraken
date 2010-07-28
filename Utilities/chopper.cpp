#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <string.h>
#include <stdint.h>


using namespace std;

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
    unsigned char* pData = NULL;
    size_t pDataSize = 0;

    if (argc<3) {
        printf("usage: %s sourcepath destpath\n",argv[0]);
        return 0;
    }

    size_t bmz = 1<<29;
#if 0
    unsigned char* pBitMap = new unsigned char[bmz];
    memset(pBitMap, 0, bmz); 
#endif

    string sSourcePath = string(argv[1]);
    string sDestPath = string(argv[2]);

    for(int i=0;i<256;i++) {
        snprintf(idx,16,"/%02x.tbl",i);

        string filename = sSourcePath + string(idx);
        printf("Reading %s\n",filename.c_str());

        FILE* fd = fopen(filename.c_str(), "r");
        if (fd==NULL) {
            printf("Error, cant open %s for reading\n",filename.c_str());
            return -1;
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
        assert(r==size);
        fclose(fd);

        size = size - (size%11);
        if (r!=size) {
            printf("Warning: file %s is not a multiple of 11 bytes long.\n",
                   filename.c_str());
        }

        unsigned char* pOut = pData;
        unsigned char* pIn = pData;
        for(int j=0; j<num; j++) {
            uint64_t idx = getIndex(pIn);
            if (idx>=8662000000ULL) {
                pIn+=11;
                continue;
            }
            if (pIn!=pOut) {
                memcpy(pOut,pIn,11);
            }
#if 0
            pBitMap[idx/8] = pBitMap[idx/8] | (unsigned char)(1<<(idx&0x7));
#endif
            pIn+=11;
            pOut+=11;
        }
        size_t outSize = pOut - pData;
        printf("In: %i chains. Out %i.\n", num, outSize/11);
        filename = sDestPath + string(idx);
        printf("Writing %s\n",filename.c_str());

        fd = fopen(filename.c_str(), "r");
        if (fd) {
            printf("Error: file exists %s will not overwrite\n",filename.c_str());
            fclose(fd);
        } else {
            fd = fopen(filename.c_str(), "w");
            if (fd==NULL) {
                printf("Error, cant open %s for writing\n",filename.c_str());
                return -1;
            }
            r = fwrite(pData,1,outSize,fd);
            fclose(fd);
            assert(r==outSize);
        }
    }

#if 0
    string filename = sDestPath + string("/bitmap.bin");
    printf("Writing %s\n",filename.c_str());

    FILE* fd = fopen(filename.c_str(), "w");
    if (fd==NULL) {
        printf("Error, cant open %s for writing\n",filename.c_str());
        return -1;
    }
        
    size_t r = fwrite(pBitMap,1,bmz,fd);
    fclose(fd);
    assert(r==bmz);
#endif

    delete [] pData;
}
