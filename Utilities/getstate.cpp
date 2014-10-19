#include "Bidirectional.h"

#include <stdio.h>
#include <iostream>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    Bidirectional back;

    if (argc==3) {
        uint64_t tst;
        uint64_t orig;
        int ticks = atoi(argv[1]);
        sscanf(argv[2],"%lux",&tst);
        orig = tst;
        std::cout << "Start: " << std::hex << tst << "\n";
        tst = back.Forwards(tst, 100+ticks, NULL);
        std::cout << std::hex << tst << "\n";
        uint64_t lfsr1 = tst & 0x7ffff;
        uint64_t lfsr2 = (tst>>19) & 0x3fffff;
        uint64_t lfsr3 = (tst>>41) & 0x7fffff;

        std::cout << "Lfsr1: " << std::hex << lfsr1 << "\n";
        std::cout << "Lfsr2: " << std::hex << lfsr2 << "\n";
        std::cout << "Lfsr3: " << std::hex << lfsr3 << "\n";

        unsigned char cipherstream[16];
        uint64_t mixed = back.Forwards(orig, 100, NULL);
        back.Forwards(mixed, 114, cipherstream);
        for (int i=0; i<15;i++) printf("%02x ",cipherstream[i]);
        printf("\n");

    }
}
