#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "Bidirectional.h"
#include "SSDlookup.h"
#include "DeltaLookup.h"
#include "../a5_cpu/A5CpuStubs.h"

#include <map>


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



int main(int argc, char* argv[] )
{
    unsigned char cipherstream[15];

    if (argc<5) {
        printf("usage %s challenge.file id index.file device\n", argv[0]);
        return 0;
    }

    int id = atoi(argv[2]);

    FILE* fd = fopen(argv[1], "rb");
    
    if (fd==NULL) {
        printf("Could not open %s for reading.\n", argv[1]);
    }

    SSDlookup tl(argv[3],argv[4]);
    // DeltaLookup tl(argv[3],argv[4]);
    // tl.SetBlockOffset(51157770ULL);
    Bidirectional back;

    printf("Table id is %i\n", id );
    /* Last argument is number of CPU cores to use */
    A5CpuInit(8, 12, 4);

    uint64_t cipher;


    std::map<uint64_t, int> searches;
    std::map<uint64_t, int>::iterator it;

    int frameno = 0;

    for(;;) {
        size_t num = fread(cipherstream, sizeof(unsigned char), 15, fd );
        if (num!=15) break;  // EOF
        
        frameno++;
        printf("------===== ANALYZING FRAME %i =====-----\n", frameno);

        for(int i=0; i<8; i++) {
            cipher = (cipher << 8) | cipherstream[i]; 
        }
        int nextbit = 64;

        /**
         * NB there are 51 possible states to examine
         * but we only do 50 for consistent coverage
         * figures. ( 50 can be changed to 51 fo 2%
         * better chance of key recovery)
         */
        for(int i=0; i<50; i++) {
            // std::cout << std::hex << "Looking at " << cipher << "\n";
            uint64_t rev = Bidirectional::ReverseBits( cipher );

            for(int j =0; j < 8; j++) {
                A5CpuSubmit(rev, j, id, NULL);
            }

            int remain = 8;
            int keysearch = 0;
            while( (remain+keysearch) > 0 ) {
                uint64_t result;
                uint64_t dummy;
                uint64_t stop_val;
                uint32_t advance;

                int start_rnd;
                if (A5CpuPopResult(dummy, stop_val, start_rnd, NULL)) {
                    it = searches.find( dummy );
                    if (it != searches.end() ) {
                        /* This was a keysearch that was issued */
                        searches.erase(it);
                        keysearch--;
                        if (start_rnd<0) {
                            stop_val = Bidirectional::ReverseBits(stop_val);
                            printf("#### Found potential key (bits: %i)####\n", i);
                            std::cout << std::hex << stop_val << "\n";
                            printf("#### Stepping back to mix ####\n");
                            stop_val = back.Forwards(stop_val, 100, NULL);
                            back.ClockBack( stop_val, 100+i );
                            printf("### Frame is %i ###\n", frameno);
                            // exit(0);
                        }
                    } else {
                        /* End point, do table lookup */
                        // std::cout << std::hex << "check " << stop_val << "\n";
                        int hits = tl.FindEndpoint(stop_val);
                        if (hits) {
                            for (int j=0;j<hits; j++) {
                                uint64_t start_search;
                                tl.PopResult(start_search);
                                // printf("Found !\n");
                                /* Issue a new keysearch */
                                uint64_t search_rev = start_search;
                                ApplyIndexFunc(search_rev, 34);
                                A5CpuKeySearch(search_rev, cipher, 0, 8, id, NULL);
                                searches[search_rev] = 0;
                                keysearch++;
                            }
                        }
                        remain--;
                    }
                }
                usleep(1000);
            }

            unsigned char bit = cipherstream[nextbit/8];

            bit = (bit>>(7-(nextbit&0x07))) & 0x01;
            cipher = (cipher << 1) | bit;
            
            nextbit++;
            fflush(stdout);
        }


    }

    sleep(10);

    fclose(fd);
}
