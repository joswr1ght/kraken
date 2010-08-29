#include "ServerCore.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h> 
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#define MAX_CLIENTS 25

ServerCore::ServerCore(int port,dispatch cb) :
    mDispatch(cb),
    mRunning(false),
    mClientCount(1)
{
    struct sockaddr_in stSockAddr;
    int reuse_addr = 1;

    mListener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(mListener==-1) {
        printf("Can't create socket.\n");
        return;
    }

	setsockopt(mListener, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
		sizeof(reuse_addr));


    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(port);
    stSockAddr.sin_addr.s_addr = INADDR_ANY;
 
    if(-1 == bind(mListener,(const struct sockaddr *)&stSockAddr,
                  sizeof(struct sockaddr_in)))
    {
        printf("error: bind failed\n");
        close(mListener);
        mListener = -1;
        return;
    }

    if(-1 == listen(mListener, 10))
    {
        printf("error: listen failed\n");
        close(mListener);
        mListener = -1;
        return;
    }

    /* Init semaphore */
    sem_init( &mMutex, 0, 1 );

    mRunning = true;
    pthread_create(&mThread, NULL, thread_stub, (void*)this);
}

ServerCore::~ServerCore()
{
    mRunning = false;
    pthread_join(mThread, NULL);
    map<unsigned int, ClientConnection*>::iterator it = mClientMap.begin();
    while(it != mClientMap.end())
    {
        delete (*it).second;
        it++;
    }
    if(mListener!=-1) {
        close(mListener);
    }
    sem_destroy(&mMutex);
}


void* ServerCore::thread_stub(void* arg)
{
    if (arg) {
        ServerCore* s = (ServerCore*)arg;
        s->Serve();
    }
    return NULL;
}



void ServerCore::Serve()
{
    fd_set sockets;
    struct timeval timeout;
    int max_desc;
    int readsocks;

    while(mRunning) {
        /* Build select list */
        FD_ZERO(&sockets);
        FD_SET(mListener, &sockets);
        max_desc = mListener;
        sem_wait(&mMutex);
        map<unsigned int, ClientConnection*>::iterator it = mClientMap.begin();
        while(it != mClientMap.end())
        {
            int fd = (*it).second->getFd();
            FD_SET(fd, &sockets);
            if (fd>max_desc) {
                max_desc = fd;
            }
            it++;
        }
        sem_post(&mMutex);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
        readsocks = select(max_desc+1, &sockets, (fd_set*)0, (fd_set*)0,
                           &timeout);

		if (readsocks < 0) {
			printf("ERROR: select failed");
			break;
		}

        if(readsocks) {
            if(FD_ISSET(mListener, &sockets)) {
                /* New connection */
                int conn = accept(mListener, NULL, NULL);
                if (mClientMap.size()<MAX_CLIENTS) {
                    ClientConnection* client = new ClientConnection(conn);
                    mClientMap[mClientCount++] = client;
                } else {
                    close(conn);
                }
            }

            sem_wait(&mMutex);
            it = mClientMap.begin();
            while(it != mClientMap.end())
            {
                ClientConnection* client = (*it).second;
                if (FD_ISSET(client->getFd(), &sockets)) {
                    string data;
                    int status = client->Read(data);
                    if (status<0) {
                        /* Error , close */
                        map<unsigned int, ClientConnection*>::iterator it2 = it;
                        it2++;
                        delete client;
                        close(client->getFd());
                        mClientMap.erase(it);
                        it = it2;
                        continue;
                    }
                    if (status>0) {
                        if (mDispatch) {
                            /* Release the mutex so dispatched calls
                             * can write back to socket.
                             * This is safe, since this is the only thread
                             * that actually modifies the client connection map
                             */
                            sem_post(&mMutex);
                            mDispatch((*it).first,data);
                            sem_wait(&mMutex);
                        }
                    }
                }
                it++;
            }
            sem_post(&mMutex);
        } 
    }
}


void ServerCore::Write(int clientId, string data)
{
    sem_wait(&mMutex);
    map<unsigned int, ClientConnection*>::iterator it
        = mClientMap.find(clientId);
    if (it!=mClientMap.end()) {
        (*it).second->Write(data);
    }
    sem_post(&mMutex);
}

void ServerCore::Broadcast(string data)
{
    sem_wait(&mMutex);
    map<unsigned int, ClientConnection*>::iterator it = mClientMap.begin();
    while(it != mClientMap.end()) {
        (*it).second->Write(data);
        it++;
    }
    sem_post(&mMutex);
}


ClientConnection::ClientConnection(int fd) :
    mFd(fd)
{
	int opts;
    /* Set non blocking */
	opts = fcntl(mFd,F_GETFL);
	if (opts < 0) {
		printf("Error while: fcntl(F_GETFL)\n");
        return;
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(mFd,F_SETFL,opts) < 0) {
		printf("Error while: fcntl(F_SETFL)\n");
        return;
	}
    // sock_mode( mFd, TCP_MODE_ASCII );
}

ClientConnection::~ClientConnection()
{
    close(mFd);
}

int ClientConnection::Write(string dat)
{
    size_t remain = dat.size();
    size_t pos = 0;
    while(remain) {
        size_t r = write(mFd, &dat.c_str()[pos], remain);
        if (r<0) break;
        remain-=r;
        pos+=r;
    }
    return 0;
}

int ClientConnection::Read(string &data)
{
    int bytes_read;
    char last_read[2] = "\0";

    errno = 0;
    bytes_read = read(mFd, &last_read, 1);
    if ((bytes_read<=0)||errno) {
        /* The other side may have closed unexpectedly */
        return -1;
    }

    if ((last_read[0]=='\r')||(last_read[0]=='\n')) {
        if (mBuffer.size()) {
            data = mBuffer;
            mBuffer = string("");
            return 1;
        }
        return 0; /* blank line */
    }

    mBuffer = mBuffer + string(last_read);
    return 1;
}

#if 0
void dump(int c, string d)
{
    printf("%i %s\n",c,d.c_str());
}


int main(int argc, char* argv[])
{
    printf("Starting\n");
    ServerCore* core = new ServerCore(4545,dump);
    sleep(10);
    core->Broadcast("Hello clients\n");
    sleep(10);
    delete core;
}
#endif
