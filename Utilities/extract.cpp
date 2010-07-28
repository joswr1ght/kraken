#include "SSDlookup.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    unsigned char cipher[16];

    if (argc<2) {
        printf("usage: %s frame\n views frame from challenge.bin\n",argv[0]);
        return 0;
    }
    FILE* fd=fopen("challenge.bin","r");
    assert(fd);
    int frame = atoi(argv[1]);
    fseek(fd,15*(frame-1),SEEK_SET);
    fread(cipher, 1,15,fd);
    fclose(fd);
    for(int i=0;i<15;i++) printf("%02x ",cipher[i]);
    printf("\n");
    return 0;
}
