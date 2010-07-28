#include "TheMatrix.h"
#include <stdio.h>
#include "Bidirectional.h"

uint64_t reverseBits(uint64_t r)
{
    uint64_t r1 = r;
    uint64_t r2 = 0;
    for (int j = 0; j < 64 ; j++ ) {
        r2 = (r2<<1) | (r1 & 0x01);
        r1 = r1 >> 1;
    }
    return r2;
}

uint64_t parity(uint64_t r)
{
    uint64_t r1 = r;

    r = (r>>32)^r;
    r = (r>>16)^r;
    r = (r>>8)^r;
    r = (r>>4)^r;
    r = (r>>2)^r;
    r = (r>>1)^r;

    return r & 0x1;
}

TheMatrix::TheMatrix()
{
    uint64_t lfsr1[19];
    uint64_t lfsr2[22];
    uint64_t lfsr3[23];

    for (int i=0; i<19; i++) lfsr1[i]=0ULL;
    for (int i=0; i<22; i++) lfsr2[i]=0ULL;
    for (int i=0; i<23; i++) lfsr3[i]=0ULL;

    uint64_t clock_in = 1;

    for (int i=0; i<64; i++) {
        uint64_t feedback1 = lfsr1[13]^lfsr1[16]^lfsr1[17]^lfsr1[18];
        uint64_t feedback2 = lfsr2[20]^lfsr2[21];
        uint64_t feedback3 = lfsr3[7]^lfsr3[20]^lfsr3[21]^lfsr3[22];

        for (int j=18; j>0; j--) lfsr1[j]=lfsr1[j-1];
        for (int j=21; j>0; j--) lfsr2[j]=lfsr2[j-1];
        for (int j=22; j>0; j--) lfsr3[j]=lfsr3[j-1];
    
        lfsr1[0] = feedback1 ^ clock_in;
        lfsr2[0] = feedback2 ^ clock_in;
        lfsr3[0] = feedback3 ^ clock_in;

        mMat1[i] = clock_in;  /* identity */
        clock_in = clock_in + clock_in; /* shift up*/
    }

    for (int i=0; i<19; i++) mMat2[i]    = lfsr1[i];
    for (int i=0; i<22; i++) mMat2[i+19] = lfsr2[i];
    for (int i=0; i<23; i++) mMat2[i+41] = lfsr3[i];

    for (int i=0; i<64; i++) mMat3[i] = mMat2[i];  /* copy for inversion */

    Invert();
}

uint64_t TheMatrix::KeyMix(uint64_t key)
{
    uint64_t out = 0;

    for (int i=0; i< 64; i++) {
        out = (out<<1) | parity(key & mMat2[i]);
    }

    return reverseBits(out);
    // return out;
}

uint64_t TheMatrix::KeyUnmix(uint64_t mix)
{
    uint64_t out = 0;

    uint64_t b = 1;
    for (int i=0; i< 64; i++) {
        out = (out<<1) | parity(mix & mMat1[i]);
    }

    return reverseBits(out);
}


uint64_t TheMatrix::KeyMixSlow(uint64_t key)
{
    uint64_t out = 0;
    int lfsr1 = 0x0;
    int lfsr2 = 0x0;
    int lfsr3 = 0x0;

    for (int i=0; i< 64; i++) {
        int bit = key & 0x01;
        key = key >> 1;

        /* Clock the different lfsr */
        unsigned int val = (lfsr1&0x52000)*0x4a000;
        val ^= lfsr1<<(31-17);
        lfsr1 = (2*lfsr1 | (val>>31)) ^ bit;

        val = (lfsr2&0x300000)*0xc00;
        lfsr2 = (2*lfsr2 | (val>>31)) ^ bit;


        val = (lfsr3&0x500080)*0x1000a00;
        val ^= lfsr3<<(31-21);
        lfsr3 = (2*lfsr3 | (val>>31)) ^ bit;
 
    }

    lfsr1 = lfsr1 & 0x7ffff;
    lfsr2 = lfsr2 & 0x3fffff;
    lfsr3 = lfsr3 & 0x7fffff;

    out = (uint64_t)lfsr1 | ((uint64_t)lfsr2<<19) | ((uint64_t)lfsr3<<41);
    return out;
}
 
uint64_t TheMatrix::CountMix(uint64_t state, uint64_t count)
{
    uint64_t out;
    unsigned int lfsr1 = state & 0x7ffff;
    unsigned int lfsr2 = (state>>19) & 0x3fffff;
    unsigned int lfsr3 = (state>>41) & 0x7fffff;

    for (int i=0; i< 22; i++) {
        unsigned int bit = count & 0x01;
        count = count >> 1;

        /* Clock the different lfsr */
        unsigned int val = (lfsr1&0x52000)*0x4a000;
        val ^= lfsr1<<(31-17);
        lfsr1 = (2*lfsr1 | (val>>31)) ^ bit;

        val = (lfsr2&0x300000)*0xc00;
        lfsr2 = (2*lfsr2 | (val>>31)) ^ bit;


        val = (lfsr3&0x500080)*0x1000a00;
        val ^= lfsr3<<(31-21);
        lfsr3 = (2*lfsr3 | (val>>31)) ^ bit;

        lfsr1 = lfsr1 & 0x7ffff;
        lfsr2 = lfsr2 & 0x3fffff;
        lfsr3 = lfsr3 & 0x7fffff;
    }
    lfsr1 = lfsr1 & 0x7ffff;
    lfsr2 = lfsr2 & 0x3fffff;
    lfsr3 = lfsr3 & 0x7fffff;

    out = (uint64_t)lfsr1 | ((uint64_t)lfsr2<<19) | ((uint64_t)lfsr3<<41);
    return out;
}

uint64_t TheMatrix::CountUnmix(uint64_t state, uint64_t count)
{
    uint64_t out;
    unsigned lfsr1 = state & 0x7ffff;
    unsigned lfsr2 = (state>>19) & 0x3fffff;
    unsigned lfsr3 = (state>>41) & 0x7fffff;

    for (int i=0; i< 22; i++) {
        unsigned int bit = count >> (21-i);

        /* Clock the different lfsr - backwards */
        unsigned int low = ((lfsr1 & 0x01) ^ bit)<<31;
        lfsr1 = lfsr1 >> 1;
        unsigned int val = (lfsr1&0x52000)*0x4a000;
        val ^= lfsr1<<(31-17);
        val = val & 0x80000000;
        lfsr1 = lfsr1 | ((val^low)>>(31-18));

        low = ((lfsr2 & 0x01) ^ bit)<<31;
        lfsr2 = lfsr2 >> 1;
        val = (lfsr2&0x300000)*0xc00;
        val = val & 0x80000000;
        lfsr2 = lfsr2 | ((val^low)>>(31-21));

        low = ((lfsr3 & 0x01) ^ bit)<<31;
        lfsr3 = lfsr3 >> 1;
        val = (lfsr3&0x500080)*0x1000a00;
        val ^= lfsr3<<(31-21);
        val = val & 0x80000000;
        lfsr3 = lfsr3 | ((val^low)>>(31-22));

    }

    out = (uint64_t)lfsr1 | ((uint64_t)lfsr2<<19) | ((uint64_t)lfsr3<<41);
    return out;
}


   
void TheMatrix::Invert()
{
    int moved[64];
    for (int i=0; i<64; i++) moved[i] = 0;

    int swaps = 1;

    /* elimination */
    uint64_t b = 1ULL;
    for (int i=0; i<64; i++) {
        for (int j=i; j<64; j++) {
            if (i==j) {
                if((mMat3[j]&b)==0) {
                    bool found = false;
                    for(int k=j; k<64; k++) {
                        if (mMat3[k]&b) {
                            mMat3[j] = mMat3[j] ^ mMat3[k];
                            mMat1[j] = mMat1[j] ^ mMat1[k];
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        /* The code doesn't handle all matrices */
                        /* abort in uncompleted state */
                        printf("Trøbbel i tårnet!\n");
                        return;
                    }
                }
            } else {
                if (mMat3[j]&b) {
                    // printf("Eliminate %i -> %i\n", i, j);
                    mMat3[j] = mMat3[j] ^ mMat3[i];
                    mMat1[j] = mMat1[j] ^ mMat1[i];
                }
            }
        }
        b = b << 1;
    }

    /* elimination */
    b = 1ULL;
    for (int i=0; i<64; i++) {
        for (int j=(i-1); j>=0; j--) {
            if (mMat3[j]&b) {
                // printf("Eliminate(2) %i -> %i\n", i, j);
                mMat3[j] = mMat3[j] ^ mMat3[i];
                mMat1[j] = mMat1[j] ^ mMat1[i];
            }
        }
        b = b << 1;
    }

}

unsigned int fn2count(unsigned int fn) {
    unsigned int t1 = fn/1326;
    unsigned int t2 = fn % 26;
    unsigned int t3 = fn % 51;
    return (t1<<11)|(t3<<5)|t2;
}


#if 0
int main(int argc, char* argv[])
{
    TheMatrix tm;
    Bidirectional back;

    // uint64_t key = 0x7AC3FFDC14451FDCULL;
    // d0 9b bc 97 1b 4c 90 00    
    // uint64_t key = 0x00904c1b97bc9bd0ULL;
    // uint64_t key = 0xd09bbc971b4c9000ULL;
    // uint64_t key = 0x005097f412ab5203ULL;
    uint64_t key = 0x0352ab12f4975000ULL;

    uint64_t mix =  tm.KeyMixSlow(key);
    uint64_t mix2 =  tm.KeyMix(key);

    printf("Mixes: %llx %llx\n", mix, mix2);
    printf("Unmix: %llx\n", key, tm.KeyUnmix(mix));
    // unsigned int count = fn2count(2597379);
    // unsigned int count = fn2count(238581);
    unsigned int count = 726;
    printf("Count: %i\n", count);
    uint64_t omix = mix;
    mix = tm.CountMix( mix, count );
    // mix = tm.CountMix( mix, c2 );
    uint64_t unmix = tm.CountUnmix( mix, count );
    printf("Mixed: %llx %llx\n", omix, unmix);

    unsigned char bytes[16];
    char out[115];
    out[114]='\0';

    uint64_t mixed = back.Forwards(mix, 1, NULL);
    printf("Mixed+1: %016llx %016llx\n", mix, mixed);
    mixed = back.Forwards(mix, 101, NULL);
        
    tm.CountUnmix(mixed,0ULL);
    mix = back.Forwards(mixed, 114, bytes);
    tm.CountUnmix(mix,0ULL);
    for (int bit=0;bit<114;bit++) {
        int byte = bit / 8;
        int b = bit & 0x7;
        int v = bytes[byte] & (1<<(7-b));
        out[bit] = v ? '1' : '0';
    }
    printf("cipher %s\n", out);

#if 0
    std::cout << "\nSlow: " << std::hex << tm.KeyMixSlow(key) << "\n";

    std::cout << "\nQuick:" << std::hex << tm.KeyMix(key) << "\n";

    uint64_t mix = tm.KeyMixSlow(key);

    uint64_t umix = tm.KeyUnmix(mix);

    std::cout << "\nUnmixed key:" << std::hex << umix << "\n\n\n";
#endif

    return 0;
}

#endif
