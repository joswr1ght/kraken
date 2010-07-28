#define _FILE_OFFSET_BITS 64

#include "MultiFileWriter.h"
#include <sys/stat.h>
#include <assert.h>

#define BUFSIZE (11*4096)

using namespace std;

MultiFileWriter::MultiFileWriter(std::string &path)
{
    mIsOK = true;
    char idx[16];
    int i;

    printf("Initializing MFWR: %s\n",path.c_str());

    struct stat st;
    if(stat(path.c_str(),&st) != 0) {
        printf("%s does not exist, attempting creation.\n",path.c_str());
        mkdir(path.c_str(),S_IRUSR|S_IWUSR|S_IXUSR);
    }

    for(i=0; i<256; i++) {
        mBuffers[i] = new unsigned char[BUFSIZE];
        mBufSize[i] = 0;
        mFiles[i] = NULL;
    }

    for(i=0; i<256; i++) {
        snprintf(idx,16,"/%02x.tbl",i);
        string filename = path + string(idx);
        FILE* fd = fopen(filename.c_str(),"r");
        if (fd!=NULL) {
            printf("ERROR: %s exists.\n", filename.c_str());
            fclose(fd);
            mIsOK = false;
            return;
        }
        fd = fopen(filename.c_str(),"w");
        if (fd==NULL) {
            printf("ERROR: can't open %s for writing.\n", filename.c_str());
            fclose(fd);
            mIsOK = false;
            return;
        }
        mFiles[i]=fd;
    }
}

MultiFileWriter::~MultiFileWriter()
{
    for(int i=0; i<256; i++) {
        if (mFiles[i]) {
            if (mBufSize[i]) {
                fwrite(mBuffers[i],1,mBufSize[i],mFiles[i]);
            }
            fclose(mFiles[i]);
        }
        delete [] mBuffers[i];
    } 
}

void MultiFileWriter::Write(uint64_t endpoint, uint64_t index)
{
    int idx = (endpoint >> 56);
    int bs = mBufSize[idx];
    unsigned char* e = &((unsigned char*)&endpoint)[1];
    unsigned char* i = (unsigned char*)&index;
    unsigned char* ch = &mBuffers[idx][bs];
    *ch++ = *e++;
    *ch++ = *e++;
    *ch++ = *e++;
    *ch++ = *e++;
    *ch++ = *e++;
    *ch++ = *e;
    *ch++ = *i++;
    *ch++ = *i++;
    *ch++ = *i++;
    *ch++ = *i++;
    *ch   = *i;
    bs += 11;
    if (bs>=BUFSIZE) {
        size_t w = fwrite(mBuffers[idx],1,BUFSIZE,mFiles[idx]);
        assert(w==BUFSIZE);
        bs = 0;
    }
    mBufSize[idx] = bs;

}
