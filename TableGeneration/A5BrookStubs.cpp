#include "A5BrookStubs.h"
#include <stdio.h>
#include <dlfcn.h>

static bool isDllLoaded = false;
static bool isDllError  = false;

bool (*fPipelineInfo)(int &length) = NULL;
bool (*fInit)(int advance, int max_rounds, int condition, int numcores) = NULL;
int  (*fSubmit)(uint64_t start_value, unsigned int start_round) = NULL;
int  (*fSubmitPartial)(uint64_t start_value, unsigned int stop_round) = NULL;
bool (*fPopResult)(uint64_t& start_value, uint64_t& stop_value, int& start_round) = NULL;
void (*fShutdown)(void) = NULL;

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

  void* lHandle = dlopen("./A5Brook.so", RTLD_LAZY | RTLD_GLOBAL);

  char* lError = dlerror();
  if (lError) {
    fprintf(stderr, "Error when opening A5Brook.so: %s\n", lError);
    return;
  }

  LoadDllSym(lHandle, "A5BrookPipelineInfo", (void**)&fPipelineInfo);
  LoadDllSym(lHandle, "A5BrookInit", (void**)&fInit);
  LoadDllSym(lHandle, "A5BrookSubmit", (void**)&fSubmit);
  LoadDllSym(lHandle, "A5BrookSubmitPartial", (void**)&fSubmitPartial);
  LoadDllSym(lHandle, "A5BrookPopResult", (void**)&fPopResult);
  LoadDllSym(lHandle, "A5BrookShutdown", (void**)&fShutdown);

  isDllLoaded = !isDllError;

}


bool A5BrookInit(int advance, int max_rounds, int condition, int numcores)
{
  LoadDLL();
  if (isDllLoaded) {
      return fInit(advance,max_rounds, condition, numcores);
  } else {
    return false;
  }
}

bool A5BrookPipelineInfo(int &length)
{
  if (isDllLoaded) {
    return fPipelineInfo(length);
  } else {
    return false;
  }
}
  
int A5BrookSubmit(uint64_t start_value, unsigned int start_round)
{
  if (isDllLoaded) {
    return fSubmit(start_value, start_round);
  } else {
    return -1;
  }
}

int  A5BrookSubmitPartial(uint64_t start_value, unsigned int stop_round)
{
  if (isDllLoaded) {
    return fSubmitPartial(start_value, stop_round);
  } else {
    return -1;
  }
}
  
bool A5BrookPopResult(uint64_t& start_value, uint64_t& stop_value, int& start_round)
{
  if (isDllLoaded) {
    return fPopResult(start_value, stop_value, start_round);
  } else {
    return false;
  }
}
  
void A5BrookShutdown()
{
  if (isDllLoaded) {
     fShutdown();
  } 
}
