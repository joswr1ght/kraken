#include "Factory.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(int argc, const char* argv[])
{
    const char* usage = "usage: %s xy file1... file2...\n"               \
        "  where xy are format codes for the input.\n"                  \
        "  m - Multifile\n"                                             \
        "  s - SSD (blockdevice + index)\n"                             \
        "  d - Delta (transitional compressed format without index.)\n" \
        "  i - Indexed Delta (experimental)\n" \
        "  h - hash (no file output, calculates internal MD5 sum)\n";

    if (argc<3) {
        printf(usage,argv[0]);
        return -1;
    }
    if (strlen(argv[1])<2) {
        printf(usage,argv[0]);
        return -1;
    }

    int consume = argc-2;

    BaseReader* reader = CreateReader(argv[1][0],&argv[2],consume);
    if (reader==NULL) {
        printf("No reader for type %c found.\n",argv[1][0]);
        return -1;
    }
    BaseWriter* writer = CreateWriter(argv[1][1],&argv[argc-consume],consume);
    if (writer==NULL) {
        printf("No reader for type %c found.\n",argv[1][1]);
        return -1;
    }

    if ((!reader->isOK())||(!writer->isOK())) {
        printf("Error opening files.\n");
        return -1;
    }

    uint64_t e,i;
    uint64_t cnt = 0;
    /* Read the first chain */
    bool more = reader->Read(e,i);
    while(more) {
        /* write + read the remaining chains */
        cnt++;
        writer->Write(e,i);
        more = reader->Read(e,i);
    }
    /* Write the last chain */
    cnt++;
    writer->Write(e,i);

    printf("%lli chains written.\n",cnt);

    delete reader;
    delete writer;
    return 0;
}
