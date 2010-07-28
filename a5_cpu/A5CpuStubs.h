#ifndef A5_CPU_STUBS
#define A5_CPU_STUBS

#include <stdint.h>

bool A5CpuInit(int max_rounds, int condition, int threads);
int  A5CpuSubmit(uint64_t start_value, int32_t start_round,
                 uint32_t advance, void* context);
int  A5CpuKeySearch(uint64_t start_value, uint64_t target,
                    int32_t start_round, int32_t stop_round,
                    uint32_t advance, void* context);
bool A5CpuPopResult(uint64_t& start_value, uint64_t& stop_value,
                    int32_t& start_round, void** context);  
void A5CpuShutdown();

#endif
