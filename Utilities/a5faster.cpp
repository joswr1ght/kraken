#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "Bidirectional.h"
#include "SSDlookup.h"
#include "../a5_cpu/A5CpuStubs.h"

#include <map>
#include <assert.h>


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
    Bidirectional back;

    printf("Table id is %i\n", id );
    /* Last argument is number of CPU cores to use */
    A5CpuInit(8, 12, 4);

    std::map<uint64_t, uint32_t> searches;
    std::map<uint64_t, uint32_t> search_count;
    std::map<uint64_t, uint32_t>::iterator it;
    std::map<uint64_t, uint64_t>::iterator it2;
    std::map<uint64_t, uint32_t>::iterator it3;
    std::map<uint64_t,uint32_t> framemap;
    std::map<uint64_t,uint64_t> ciphermap;

    int frameno = 0;
    int remain = 0;
    int keysearch = 0;

    for(;;) {
        uint64_t cipher;

        size_t num = fread(cipherstream, sizeof(unsigned char), 15, fd );
        if (num!=15) break;  // EOF
        frameno++;

        // if (frameno<9958) continue;
        // if (frameno>5) break;

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
        for(int i=0; i<51; i++) {
            // std::cout << std::hex << "Looking at " << cipher << "\n";
            uint64_t rev = Bidirectional::ReverseBits( cipher );

            for(int j =0; j < 8; j++) {
                A5CpuSubmit(rev, j, id, NULL);
                remain++;
            }
            it = framemap.find(rev);
            assert(it==framemap.end());
            framemap[rev] = frameno*8*64+i*8+7;
            ciphermap[rev] = cipher;

            unsigned char bit = cipherstream[nextbit/8];

            bit = (bit>>(7-(nextbit&0x07))) & 0x01;
            cipher = (cipher << 1) | bit;
            
            nextbit++;
            fflush(stdout);
        }
    }

    int numseeks = 0;
    printf("Churning the cipher.\n");

    while( (remain+keysearch) > 0 ) {
        uint64_t result;
        uint64_t dummy;
        uint64_t stop_val;
        int start_rnd;
        if (A5CpuPopResult(dummy, stop_val, start_rnd, NULL)) {
            it = searches.find( dummy );
            if (it != searches.end() ) {
                /* This was a keysearch that was issued */
                keysearch--;
                frameno = (it->second)/8;
                int i = frameno&0x3f;
                frameno = frameno/64;
                if (start_rnd<0) {
                    stop_val = Bidirectional::ReverseBits(stop_val);
                    printf("#### Found potential key (bits: %i)####\n", i);
                    std::cout << std::hex << stop_val << "\n";
                    printf("#### Stepping back to mix ####\n");
                    stop_val = back.Forwards(stop_val, 100, NULL);
                    back.ClockBack( stop_val, 100+i );
                    printf("### Frame is %i ###\n", frameno);
                }
                it3 = search_count.find(dummy);
                assert(it3!=search_count.end());
                int count = it3->second - 1;
                if (count) {
                    it3->second = count;
                } else {
                    search_count.erase(it3);
                    searches.erase(it);
                }
            } else {
                /* End point, do table lookup */
                // std::cout << std::hex << "check " << stop_val << "\n";
                int hits = tl.FindEndpoint(stop_val);
                it = framemap.find(dummy);
                it2 = ciphermap.find(dummy);
                assert(it2!=ciphermap.end());
                assert(it!=framemap.end());
                numseeks++;
                uint32_t cnt = (it->second&0x07) - 1;
                if (cnt<0) {
                    framemap.erase(it);
                } else {
                    it->second = (it->second & 0xfffffff8) + cnt;
                }
                if (hits) {
                    for (int j=0;j<hits; j++) {
                        uint64_t start_search;
                        tl.PopResult(start_search);
                        /* Issue a new keysearch */
                        uint64_t search_rev = start_search;
                        ApplyIndexFunc(search_rev, 34);
                        A5CpuKeySearch(search_rev, it2->second, 0, 8, id, NULL );
                        searches[search_rev] = it->second;
                        it = framemap.find(search_rev);
                        assert(it==framemap.end());
                        /* Keep track of how many searches are issued for this
                         * piece of plaintext. */
                        it = search_count.find( search_rev );
                        if (it!=search_count.end()) {
                            int count = it->second;
                            it->second = count + 1;
                        } else {
                            search_count[search_rev] = 1;
                        }
                        keysearch++;
                    }
                }
                remain--;
            }
        }
        usleep(1000);
    }

    // printf("%i seeks to disk performed\n",numseeks);

    fclose(fd);
}
