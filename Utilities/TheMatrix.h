#include <stdint.h>

class TheMatrix {
public:
    TheMatrix();

    uint64_t KeyMix(uint64_t);
    uint64_t KeyUnmix(uint64_t);
    uint64_t KeyMixSlow(uint64_t);

    uint64_t CountMix(uint64_t, uint64_t);
    uint64_t CountUnmix(uint64_t, uint64_t );

    uint64_t mMat1[64];
    uint64_t mMat2[64];
    uint64_t mMat3[64];
private:
    void Invert();
};

