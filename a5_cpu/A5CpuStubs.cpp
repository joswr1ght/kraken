#include "A5CpuStubs.h"
#include <stdio.h>
#include <dlfcn.h>

static bool isDllLoaded = false;
static bool isDllError  = false;


static bool (*fInit)(int max_rounds, int condition, int threads) = NULL;
static int  (*fSubmit)(uint64_t start_value, int32_t start_round, uint32_t advance, void* context) = NULL;
static int  (*fKeySearch)(uint64_t start_value, uint64_t target, int32_t start_round, int32_t stop_round,
                   uint32_t advance, void* context) = NULL;
static bool (*fPopResult)(uint64_t& start_value, uint64_t& stop_value,
                   int32_t& start_round, void** context) = NULL;
static void (*fShutdown)(void) = NULL;

static bool LoadDllSym(void* handle, const char* name, void** func)
{
  *func = dlsym(handle, name);
  char* lError = dlerror();
  if (lError) {
    fprintf(stderr, "Error when loading symbol (%s): %s\n", name, lError);
    isDllError = true;
    return false;
  }
  return true;
}

static void LoadDLL(void)
{
  if (isDllError) return;

  void* lHandle = dlopen("./A5Cpu.so", RTLD_LAZY | RTLD_GLOBAL);

  char* lError = dlerror();
  if (lError) {
    fprintf(stderr, "Error when opening A5Cpu.so: %s\n", lError);
    return;
  }

  LoadDllSym(lHandle, "A5CpuInit", (void**)&fInit);
  LoadDllSym(lHandle, "A5CpuSubmit", (void**)&fSubmit);
  LoadDllSym(lHandle, "A5CpuKeySearch", (void**)&fKeySearch);
  LoadDllSym(lHandle, "A5CpuPopResult", (void**)&fPopResult);
  LoadDllSym(lHandle, "A5CpuShutdown", (void**)&fShutdown);

  isDllLoaded = !isDllError;

}


bool A5CpuInit(int max_rounds, int condition, int threads)
{
  LoadDLL();
  if (isDllLoaded) {
    return fInit(max_rounds, condition, threads);
  } else {
    return false;
  }
}
  
int  A5CpuSubmit(uint64_t start_value, int32_t start_round, uint32_t advance, void* context)
{
  if (isDllLoaded) {
      return fSubmit(start_value, start_round, advance, context);
  } else {
    return -1;
  }
}

int  A5CpuKeySearch(uint64_t start_value, uint64_t target, int32_t start_round,
                    int32_t stop_round, uint32_t advance, void* context)
{
  if (isDllLoaded) {
      return fKeySearch(start_value, target, start_round, stop_round, advance, context);
  } else {
      return -1;
  }
}
  
bool A5CpuPopResult(uint64_t& start_value, uint64_t& stop_value,
                    int32_t& start_round, void** context)
{
  if (isDllLoaded) {
      return fPopResult(start_value, stop_value, start_round, context);
  } else {
    return false;
  }
}
  
void A5CpuShutdown()
{
  if (isDllLoaded) {
     fShutdown();
  } 
}
