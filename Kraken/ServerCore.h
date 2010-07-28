/***********************************************
 *
 * Simple single threaded servercore
 *
 **********************************************/

#include <string>
#include <pthread.h>
#include <map>
#include <semaphore.h>

using namespace std;

typedef void (*dispatch)(int, std::string);

class ClientConnection {
public:
    ClientConnection(int);
    ~ClientConnection();

    int getFd() {return mFd;}
    int Write(string dat);
    int Read(string& data);

private:
    int mFd;
    string mBuffer;
};


class ServerCore {
public:
    ServerCore(int,dispatch);
    ~ServerCore();

    void Write(int, string);
    void Broadcast(string);

private:
    void Serve();

    bool mRunning;
    static void* thread_stub(void* arg);
    pthread_t mThread;
    dispatch mDispatch;

    int mListener;
    unsigned int mClientCount;
    map<unsigned int, ClientConnection*> mClientMap;
    sem_t mMutex;
};

