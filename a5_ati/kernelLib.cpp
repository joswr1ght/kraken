#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <stdlib.h>

extern "C" {

#include "zlib.h"

extern unsigned int _binary_my_kernel_dp11_Z_end;
extern unsigned int _binary_my_kernel_dp11_Z_size;
extern unsigned int _binary_my_kernel_dp11_Z_start;

extern unsigned int _binary_my_kernel_dp12_Z_end;
extern unsigned int _binary_my_kernel_dp12_Z_size;
extern unsigned int _binary_my_kernel_dp12_Z_start;

extern unsigned int _binary_my_kernel_dp13_Z_end;
extern unsigned int _binary_my_kernel_dp13_Z_size;
extern unsigned int _binary_my_kernel_dp13_Z_start;

extern unsigned int _binary_my_kernel_dp14_Z_end;
extern unsigned int _binary_my_kernel_dp14_Z_size;
extern unsigned int _binary_my_kernel_dp14_Z_start;
}


static char* gKernelStarts[4] = {
    (char*)&_binary_my_kernel_dp11_Z_start,
    (char*)&_binary_my_kernel_dp12_Z_start,
    (char*)&_binary_my_kernel_dp13_Z_start,
    (char*)&_binary_my_kernel_dp14_Z_start
};

static char* gKernelEnds[4] = {
    (char*)&_binary_my_kernel_dp11_Z_end,
    (char*)&_binary_my_kernel_dp12_Z_end,
    (char*)&_binary_my_kernel_dp13_Z_end,
    (char*)&_binary_my_kernel_dp14_Z_end
};

#define CHUNK 1024

unsigned char* getKernel(unsigned int dp)
{
    int ret;

    dp -= 11;
    if ((dp<0)||(dp>3)) return NULL;

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = gKernelEnds[dp]-gKernelStarts[dp];
    size_t bsize = CHUNK + 1;
    void *buf = malloc(bsize);
    size_t ind = 0;
    strm.next_out = (Bytef*)buf;
    strm.next_in = (Bytef*)gKernelStarts[dp];
    ret = inflateInit(&strm);
    if (ret != Z_OK) return NULL;

    do {
        if ((ind+CHUNK)>bsize) {
            bsize += CHUNK;
            buf = realloc(buf, bsize);
        }
        strm.avail_out = CHUNK;
        strm.next_out = &((Bytef*)buf)[ind];

        ret = inflate(&strm, Z_NO_FLUSH);    /* no bad return value */
        assert(ret != Z_STREAM_ERROR);       /* state not clobbered */
        switch (ret) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            printf("Zlib error: %i\n", ret);
            (void)inflateEnd(&strm);
            free(buf);
            return NULL;
        default:
            break;
        }
        size_t have = CHUNK - strm.avail_out;
        ind += have;

        // printf("Have %i bytes: in %i\n", have, strm.avail_in);
    
    } while (strm.avail_out == 0);

    inflateEnd(&strm);
    ((char*)buf)[ind]=0;

    return (unsigned char*)buf;
}

void freeKernel(unsigned char* k)
{
    free(k);
}

#if 0
int main(int argc, char* argv[])
{
    if (argc>1) {
        printf("%s", getKernel(atoi(argv[1])));
    } else {
        printf("%s", getKernel(11));
    }
}
#endif
