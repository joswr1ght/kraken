#include "Bidirectional.h"

#include <stdio.h>
#include <iostream>

int main(int argc, char* argv[])
{
    long long out = 0;
    int lfsr1 = 0x1234;
    int lfsr2 = 0x5678;
    int lfsr3 = 0x9abc;

    Bidirectional back;

    if (argc==3) {
        unsigned int a,b;
        sscanf(argv[2],"%x:%x",&a,&b);
        // printf("%08x %08x\n",a,b);
        // uint64_t tst = back.Forwards(0x123456789abcdef0ULL, 230, NULL);
        uint64_t tst = ((uint64_t)a)<<32|b;
        // uint64_t tst = back.Forwards(0x8680a4cec29e70f5ULL, 110, NULL);
        std::cout << std::hex << tst << "\n";
        back.ClockBack( tst , 110 );
        std::cout << std::hex << tst << "\n";
    } else {
        FILE *fd = fopen(argv[1],"wb");
        if (fd==0) {
            printf("Can't write to %s\n", argv[1]);
            return -1;
        }
        FILE* rnd = fopen("/dev/urandom", "rb");
        if (rnd==0) {
            printf("Can't read /dev/urandom\n");
            fclose(fd);
            return -1;
        }

        for (int i=0; i < 1000; i++) {
            unsigned char cipherstream[15];
            uint64_t key = 0xf5a472fcfa67c694ULL;
            fread( &key, sizeof(uint64_t), 1 , rnd);
            uint64_t mixed = back.Forwards(key, 100, NULL);
            back.Forwards(mixed, 114, cipherstream);

            fwrite(cipherstream,sizeof(unsigned char),15,fd);
        }
        fclose(rnd);
        fclose(fd);
    }

#if 0

    printf("\nFinal state: %x %x %x\n", lfsr1, lfsr2, lfsr3); 
    std::cout << std::hex << out << "\n";

#endif

}
