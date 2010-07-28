#include "BaseReader.h"
#include "BaseWriter.h"

BaseReader* CreateReader(char type, const char* args[], int &consumed);

BaseWriter* CreateWriter(char type, const char* args[], int &consumed);

