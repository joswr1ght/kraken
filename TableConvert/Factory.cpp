#include "DeltaReader.h"
#include "DeltaWriter.h"
#include "MultiFileReader.h"
#include "MultiFileWriter.h"
#include "SSDreader.h"
#include "SSDwriter.h"
#include "Md5Writer.h"
#include "IndexWriter.h"

BaseReader* CreateReader(char type, const char* args[], int &consumed)
{
    switch(type) {
    case 'm':
        if (consumed>=1) {
            consumed--;
            std::string name(args[0]);
            return new MultiFileReader(name);
        }
        break;
    case 'd':
        if (consumed>=1) {
            consumed--;
            std::string name(args[0]);
            return new DeltaReader(name);
        }
        break;
    case 's':
        if (consumed>=2) {
            consumed-=2;
            std::string name(args[0]);
            std::string iname(args[1]);
            return new SSDreader(name,iname);
        }
        break;
    default:
        break;
    }

    return NULL;
}

BaseWriter* CreateWriter(char type, const char* args[], int &consumed)
{
    switch(type) {
    case 'h':
        return new Md5Writer();
        break;
    case 'm':
        if (consumed>=1) {
            consumed--;
            std::string name(args[0]);
            return new MultiFileWriter(name);
        }
        break;
    case 'd':
        if (consumed>=1) {
            consumed--;
            std::string name(args[0]);
            return new DeltaWriter(name);
        }
        break;
    case 'i':
        if (consumed>=2) {
            consumed-=2;
            std::string name(args[0]);
            std::string iname(args[1]);
            return new IndexWriter(name,iname,4096);
        }
        break;
    case 's':
        if (consumed>=2) {
            consumed-=2;
            std::string name(args[0]);
            std::string iname(args[1]);
            return new SSDwriter(name,iname);
        }
        break;
    default:
        break;
    }

    return NULL;   
}
