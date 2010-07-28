#include "Bidirectional.h"

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main(int argc, char* argv[])
{
    Bidirectional back;

    int frame = -1;
    bool match = false;
    unsigned char search_cipher[16];

    if (argc>=4) {
        sscanf(argv[3],"%i",&frame);
        FILE* fd=fopen("challenge.bin","r");
        assert(fd);
        fseek(fd,15*(frame-1),SEEK_SET);
        fread(search_cipher, 1,15,fd);
        fclose(fd);
        printf("Input ciper stream (%i): ",frame);
        for (int i=0; i<15;i++) printf("%02x ",search_cipher[i]);
        printf("\n");
    }


    if (argc>=3) {
        uint64_t tst;
        int ticks = atoi(argv[1]);
        sscanf(argv[2],"%llx",&tst);
        std::cout << "Start: " << std::hex << tst << "\n";
        tst = back.Forwards(tst, 100, NULL);
        printf("Stepping back 100 + %i ticks.\n", ticks);
        std::cout << std::hex << tst << "\n";
        back.ClockBack( tst , ticks+100 );
        while (back.PopCandidate(tst)) {
            unsigned char cipherstream[16];
            std::cout << std::hex << tst << "\n";
            uint64_t mixed = back.Forwards(tst, 100, NULL);
            back.Forwards(mixed, 114, cipherstream);
            for (int i=0; i<15;i++) printf("%02x ",cipherstream[i]);
            if ((frame>=0)&&(!match)) {
                match = (memcmp(search_cipher,cipherstream,15)==0);
            }
            printf("\n");
        }
    }

    if (frame>=0) {
        printf( match ? "Complete match\n" : "partial match, false hit\n" );
    }

}
