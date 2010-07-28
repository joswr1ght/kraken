#ifndef A5_BROOK_STUBS
#define A5_BROOK_STUBS

#include <stdint.h>

bool A5BrookPipelineInfo(int &length);
bool A5BrookInit(int advance, int max_rounds, int condition, bool use_tables);
int  A5BrookSubmit(uint64_t start_value, unsigned int start_round);
int  A5BrookSubmitPartial(uint64_t start_value, unsigned int stop_round);
bool A5BrookPopResult(uint64_t& start_value, uint64_t& stop_value, int& start_round);  
void A5BrookShutdown();

void ApplyIndexFunc(uint64_t& start_index, int bits);
int ExtractIndex(uint64_t& start_value, int bits);

#endif
