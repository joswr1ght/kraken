#include <stdio.h>
#include "Bidirectional.h"
#include "TheMatrix.h"
#include <string.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{

    if (argc<4) {
        printf("usage: %s foundkey bitpos framecount (framecount2 burst2)\n", argv[0]);
        return -1;
    }

    unsigned framecount = 0;
    uint64_t stop;
    sscanf(argv[1],"%llx",&stop);
    int pos;
    sscanf(argv[2],"%i",&pos);
    Bidirectional back;
    TheMatrix tm;
    back.doPrintCand(false);
    sscanf(argv[3],"%i",&framecount);

    uint64_t stop_val = Bidirectional::ReverseBits(stop);
    printf("#### Found potential key (bits: %i)####\n", pos);
    stop_val = back.Forwards(stop_val, 100, NULL);
    back.ClockBack( stop_val, 101+pos );
    uint64_t tst;
    unsigned char bytes[16];
    char out[115];
    out[114]='\0';
    int x = 0;
    printf("Framecount is %i\n", framecount);

    unsigned framecount2 = -1;
    if (argc>=6) {
	if (strlen(argv[5]) != 114) {
		fprintf(stderr, "burst2 must be a 114 digit bitstring\n");
		exit(1);
	}
        sscanf(argv[4],"%i",&framecount2);
    }

    while (back.PopCandidate(tst)) {
        uint64_t orig = tm.CountUnmix(tst, framecount);
        orig = tm.KeyUnmix(orig);
        printf("KC(%i): ", x);
        for(int i=7; i>=0; i--) {
            printf("%02x ",(unsigned)(orig>>(8*i))&0xff);
        }
        x++;

        if (framecount2>=0) {
            uint64_t mix = tm.KeyMix(orig);
            mix = tm.CountMix(mix,framecount2);
            mix = back.Forwards(mix, 101, NULL);
            back.Forwards(mix, 114, bytes);
            int ok = 0;
            for (int bit=0;bit<114;bit++) {
                int byte = bit / 8;
                int b = bit & 0x7;
                int v = bytes[byte] & (1<<(7-b));
                char check = v ? '1' : '0';
                if (check==argv[5][bit]) ok++;
            }
            if (ok>104) {
                printf(" *** MATCHED ***");
            } else {
                printf(" mismatch");
            }
        }

        printf("\n");

#if 0
        uint64_t mixed = back.Forwards(tst, 101, NULL);
        back.Forwards(mixed, 114, bytes);
        for (int bit=0;bit<114;bit++) {
            int byte = bit / 8;
            int b = bit & 0x7;
            int v = bytes[byte] & (1<<(7-b));
            out[bit] = v ? '1' : '0';
        }
        printf("cipher %s\n", out);
#endif
    }
}
