#include "Kraken.h"
#include "Fragment.h"
#include <stdio.h>
#include <string>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../a5_cpu/A5CpuStubs.h"
#include "../a5_ati/A5AtiStubs.h"

Kraken* Kraken::mInstance = NULL;

/**
 *  Create a singleton like instance of Kraken
 *  Loads all table indexes into memory
 */
Kraken::Kraken(const char* config, int server_port) :
    mNumDevices(0)
{
    assert (mInstance==NULL);
    mInstance = this;

    string configFile = string(config)+string("/tables.conf");
    FILE* fd = fopen(configFile.c_str(),"r");
    if (fd==NULL) {
        printf("Could not find %s\n", configFile.c_str());
        assert(0);
    }
    fseek(fd ,0 ,SEEK_END );
    int size = ftell(fd);
    fseek(fd ,0 ,SEEK_SET );
    char* pFile = new char[size+1];
    size_t r = fread(pFile,1,size,fd);
    pFile[size]='\0';
    assert(r==size);
    fclose(fd);
    for (int i=0; i < size; i++) {
        if (pFile[i]=='\r') {
            pFile[i] ='\0';
            continue;
        }
        if (pFile[i]=='\n') pFile[i] ='\0';
    }
    int pos = 0;
    while (pos<size) {
        if (pFile[pos]) {
            int len = strlen(&pFile[pos]);
            if (strncmp(&pFile[pos],"Device:",7)==0) {
                printf("%s\n", &pFile[pos]);
                const char* ch1 = strchr(&pFile[pos],' ');
                if (ch1) {
                    ch1++;
                    const char* ch2 = strchr(ch1,' ');
                    string devname(ch1,ch2-ch1);
                    printf("%s\n", devname.c_str());
                    mNumDevices++;
                    mDevices.reserve(mNumDevices);
                    mDevices.push_back(new NcqDevice(devname.c_str()));
                }
            }
            else if (strncmp(&pFile[pos],"Table:",6)==0) {
                unsigned int devno;
                unsigned int advance;
                uint64_t offset;
                sscanf(&pFile[pos+7],"%u %u %luu",&devno,&advance,&offset);
                // printf("%u %u %llu\n", devno, advance, offset );
                assert(devno<mNumDevices);
                char num[32];
                sprintf( num,"/%u.idx", advance );
                string indexFile = string(config)+string(num);
                // if (advance==372)
                {
                    char num[16];
                    DeltaLookup* dl = new DeltaLookup(mDevices[devno],indexFile);
                    dl->setBlockOffset(offset);
                    mTables.push_back( pair<unsigned int, DeltaLookup*>(advance, dl) );
                    /* Add to TableInfo list */
                    if (mTableInfo.size()) {
                        snprintf(num,16,",%d",advance);
                        mTableInfo = mTableInfo+string(num);
                    } else {
                        snprintf(num,16,"%d",advance);
                        mTableInfo = string(num);
                    }
                    /* Make an entry in the active map */
                    mActiveMap[advance]=1;
                }
            }
            pos += len;
        }
        pos++;
    }
    mTableInfo = string("Tables: ")+mTableInfo+string("\n");
    printf("%s",mTableInfo.c_str());

    delete [] pFile;

    /* Init semaphore */
    sem_init( &mMutex, 0, 1 );

    A5CpuInit(8, 12, 4);
    
    fd = fopen("A5Ati.so","r");
    if (fd) {
        fclose(fd);
        mUsingAti = true;
        // Use 1 GPU (first)
        // A5AtiInit(8,12,0x1,1);
        // Use all GPUs
        A5AtiInit(8,12,0xffffffff,1);
    } else {
        mUsingAti = false;
    }

    mBusy = false;
    mServer = NULL;
    if (server_port) {
        mServer = new ServerCore(server_port, serverCmd);
    }
    mJobCounter = 0;
}

/**
 *  Cleanup, and free everything.
 */
Kraken::~Kraken()
{
    tableListIt it = mTables.begin();
    while (it!=mTables.end()) {
        delete (*it).second;
        it++;
    }

    for (int i=0; i<mNumDevices; i++) {
        delete mDevices[i];
    }

    A5CpuShutdown();

    if (mUsingAti) {
        A5AtiShutdown();
    }

    sem_destroy(&mMutex);
}

/**
 *  Recieve a crack command and insert into mutex protected queue.
 */
void Kraken::Crack(int client, const char* plaintext)
{
    sem_wait(&mMutex);
    mWorkOrders.push(string(plaintext));
    mWorkClients.push(client);
    sem_post(&mMutex);
}

/**
 *  Main woker thread loop
 */
bool Kraken::Tick()
{
    uint64_t start_val;
    uint64_t stop_val;
    Fragment* frag;
    int start_rnd;

    while (A5CpuPopResult(start_val, stop_val, start_rnd, (void**)&frag)) {
        frag->handleSearchResult(stop_val, start_rnd);
    }

    if (mUsingAti) {
        while (A5AtiPopResult(start_val, stop_val, (void**)&frag)) {
            frag->handleSearchResult(stop_val, 0);
        }
    }

    sem_wait(&mMutex);
    if (mWorkOrders.size()>0) {
        /* Start a new job */
        string work = mWorkOrders.front();
        mBusy = true;
        mWorkOrders.pop();
        int client = mWorkClients.front();
        mWorkClients.pop();
        const char* plaintext = work.c_str();
        if (client && mServer) {
            char msg[32];
            snprintf(msg,32,"Cracking #%i ", mJobCounter);
            string txt = string(msg)+work+string("\n");
            mServer->Write(client, txt);
        }
        printf("Cracking %s\n", plaintext );

        /* Set the active map to indicate which tables shall be used */
        const char* tablist = strchr( plaintext, '[' );
        if (tablist) {
            /* Start with no active */
            map<unsigned int,int>::iterator it = mActiveMap.begin();
            while (it!=mActiveMap.end()) {
                (*it).second = 0;
                it++;
            }
            tablist++;
            unsigned int num;
            while(sscanf(tablist,"%d",&num)==1) {
                it = mActiveMap.find(num);
                if (it!=mActiveMap.end()) {
                    (*it).second = 1;
                }
                while ((*tablist>='0')&&(*tablist<='9')) {
                    tablist++;
                }
                if (*tablist==',') {
                    tablist++;
                } else break;
            }
        } else {
            /* All active */
            map<unsigned int,int>::iterator it = mActiveMap.begin();
            while (it!=mActiveMap.end()) {
                (*it).second = 1;
                it++;
            }
        }


        /* Count samples that may be checked from known plaintext */
        int samples = 0;
        const char* ch = plaintext;
        while( *ch=='0' || *ch=='1') {
            ch++;
            samples++;
        }
        samples -= 63;
        int submitted = 0;
        for (int i=0; i<samples; i++) {
            uint64_t plain = 0;
            uint64_t plainrev = 0;
            for (int j=0;j<64;j++) {
                if (plaintext[i+j]=='1') {
                    plain = plain | (1ULL<<j);
                    plainrev = plainrev | (1ULL<<(63-j));
                }
            }
            tableListIt it = mTables.begin();
            while (it!=mTables.end()) {
                /* Skip if not active */
                if (mActiveMap[(*it).first]==0) {
                    it++;
                    continue;
                }
                /* Create fragments for sample */ 
                for (int k=0; k<8; k++) {
                    Fragment* fr = new Fragment(plainrev,k,(*it).second,(*it).first);
                    fr->setBitPos(i);
                    fr->setJobNum(mJobCounter);
                    fr->setClientId(client);
                    mFragments[fr] = 0;
                    if (mUsingAti) {
                        A5AtiSubmit(plain, k, (*it).first, fr);
                    } else {
                        A5CpuSubmit(plain, k, (*it).first, fr);
                    }
                    submitted++;
                }
                it++;
            }
        }
        if (submitted) {
            struct timeval start_time;
            gettimeofday(&start_time, NULL);
            mTimingMap[mJobCounter] = start_time;
            mJobMap[mJobCounter] = submitted;
        } else {
            /* No job was actually started - report completion */
            char msg[128];
            snprintf(msg,128,"crack #%i took 0 msec\n",mJobCounter);
            printf("%s",msg);
            if (client&&mServer) {
                mServer->Write(client, string(msg));
            }
        }
        mJobCounter++;
    } else if (mJobMap.size()==0) {
        mBusy = false;
    }
    sem_post(&mMutex);
    return mBusy;
}

/**
 *  Remove and delete a fragment from the work list maps
 */
void Kraken::removeFragment(Fragment* frag)
{
    sem_wait(&mMutex);
    map<unsigned int,int>::iterator it2 = mJobMap.find(frag->getJobNum());
    if (it2!=mJobMap.end()) {
        int num = (*it2).second - 1;
        if (num) {
            (*it2).second = num;
        } else {
            map<unsigned int,struct timeval>::iterator it3 =
                mTimingMap.find(frag->getJobNum());
            if (it3!=mTimingMap.end()) {
                char msg[128];
                struct timeval tv;
                gettimeofday(&tv, NULL);
                struct timeval start_time = (*it3).second;
                unsigned long diff = 1000000*(tv.tv_sec-start_time.tv_sec);
                diff += tv.tv_usec-start_time.tv_usec;
                snprintf(msg,128,"crack #%i took %i msec\n",frag->getJobNum(),
                         (int)(diff/1000));
                printf("%s",msg);
                int client = frag->getClientId();
                if (client&&mServer) {
                    mServer->Write(client, string(msg));
                }
                mTimingMap.erase(it3);
            }
            mJobMap.erase(it2);
        }
    }
    map<Fragment*,int>::iterator it = mFragments.find(frag);
    if (it!=mFragments.end()) {
        mFragments.erase(it);
        delete frag;
    }
    sem_post(&mMutex);
}

void Kraken::showFragments()
{
    sem_wait(&mMutex);
    map<unsigned int,int>::iterator it = mJobMap.begin();
    printf("Active jobs[");
    while (it!=mJobMap.end()) {
        printf("%d ", (*it).first );
        it++;
    }
    unsigned int histogram[4];
    histogram[0] = 0;
    histogram[1] = 0;
    histogram[2] = 0;
    histogram[3] = 0;
    int total = 0;
    map<Fragment*,int>::iterator it2 = mFragments.begin();
    while (it2!=mFragments.end()) {
        int state = (*it2).first->getState();
        if ((state>=0)&&(state<4)) {
            histogram[state]++;
            total++;
        }
        // printf("frag %p %d\n",(*it2).first, state);
        it2++;
    }
    sem_post(&mMutex);
    printf("] state counts (%d): %d %d %d %d\n", total, 
           histogram[0],histogram[1],histogram[2],histogram[3]);
}

/**
 * Report a found key back to the issuing client
 */
void Kraken::reportFind(string found, int client)
{
    if (client && mServer) {
        mServer->Write(client, found);
    }
}

/**
 *  Recieve and parse commands from clients
 */
void Kraken::serverCmd(int clientID, string cmd)
{
    const char* command = cmd.c_str();
    if (strncmp(command,"crack",5)==0) {
        const char* ch = command+5;
        while (*ch && (*ch!='0') && (*ch!='1')) ch++;
        int len = strlen(ch);
        if (len>63) {
            getInstance()->Crack(clientID, ch);
        }
    } else if (strncmp(command,"list",4)==0) {
        /* Return a printed list of loaded tables */
        printf("%s",mInstance->mTableInfo.c_str());
        mInstance->mServer->Write(clientID, mInstance->mTableInfo);
    }
}

/**
 * Program entry point
 */
int main(int argc, char* argv[])
{
    if (argc<2) {
        printf("usage: %s index_path (server port)\n", argv[0] );
        return -1;
    }

    char command[256];
    int server_port = 0;
    if (argc>2) server_port=atoi(argv[2]);
    Kraken kr(argv[1], server_port);

    struct timeval debug_timer;
    struct timeval dbg2;
    gettimeofday(&debug_timer, NULL);

    printf("Commands are: crack test quit\n");
    for (;;) {
        bool busy = kr.Tick();
        usleep(500);
        
#if 0
        // Debug progress over time.
        if (busy) {
            gettimeofday(&dbg2, NULL);
            unsigned long diff = 1000000*(dbg2.tv_sec-debug_timer.tv_sec);
            diff += dbg2.tv_usec-debug_timer.tv_usec;
            if (diff>100000) {
                debug_timer = dbg2;
                kr.showFragments();
            }
        }
#endif

        if (!busy && server_port==0) {
            printf("\nKraken> ");
            char* ch = fgets(command, 256 , stdin);
            command[255]='\0';
            if (!ch) break;
            size_t len = strlen(command);
            if (command[len-1]=='\n') {
                len--;
                command[len]='\0';
            }
            printf("\n");

            if (strncmp(command,"test",4)==0) {
                // Test frame 998
                kr.Crack(0, "001101110011000000001000001100011000100110110110011011010011110001101010100100101111111010111100000110101001101011"); 
            } else if (strncmp(command,"crack",5)==0) {
                ch = command+5;
                while (*ch && (*ch!='0') && (*ch!='1')) ch++;
                len = strlen(ch);
                if (len>63) {
                    kr.Crack(0, ch);
                }
            } else if (strncmp(command,"quit",4)==0) {
                printf("Quitting.\n");
                break;
            }
        }
    }
}
