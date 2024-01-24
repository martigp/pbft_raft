#include <string>
#include <sys/event.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "RaftServer/SocketManager.hh"
#include "RaftServer/Socket.hh"

namespace Raft {
    
    SocketManager::SocketManager( Globals& globals )
        : globals(globals)
    {   
        kq = kqueue();
        if (kq == -1) {
            perror("Failed to create kqueue");
            exit(EXIT_FAILURE);
        }
    }

    SocketManager::~SocketManager()
    {
        if (close(kq) == -1) {
            perror("Failed to kqueue");
            exit(EXIT_FAILURE);
        }
    }

    bool SocketManager::registerSocket( Socket *socket ) {

        struct kevent newEv;
        EV_SET(&newEv, socket->fd, EVFILT_READ, EV_ADD, 0, 0, socket);

        printf("About to register new event on kq %d\n", kq);

        if (kevent(kq, &newEv, 1, NULL, 0, NULL) == -1) {
            perror("Failure to register client socket");
            return false;
        }
        printf("Registered new event\n");
        return true;
    }

    bool SocketManager::removeSocket( Socket* socket ) {
        struct kevent ev;

        /* Set flags for an event to stop the kernel from listening for
        events on this socket. */
        EV_SET(&ev, socket->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);

        if (kevent(kq, &ev, 1, NULL, 0, NULL) == -1) {
            perror("Failure to remove client socket");
            return false;
        }

        delete socket;

        return true;
    }

    ClientSocketManager::ClientSocketManager( Raft::Globals& globals )
        : SocketManager( globals )
    {        
    }

    ClientSocketManager::~ClientSocketManager()
    {
    }

    void ClientSocketManager::start()
    {}

    ServerSocketManager::ServerSocketManager( Raft::Globals& globals )
        : SocketManager( globals )
    {
        int listenSocketFd;
        struct sockaddr_in listenSockAddr;
        int opt = 1;

        // Creating socket file descriptor
        if ((listenSocketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }
    
        // Forcefully attaching socket to the port 8080
        if (setsockopt(listenSocketFd, SOL_SOCKET, SO_REUSEADDR, &opt,
                    sizeof(opt))) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        // Construct listen sockadrr_in from configuration parameters
        listenSockAddr.sin_family = AF_INET;
        listenSockAddr.sin_port = htons(globals.config.raftPort);

        if (inet_pton(AF_INET, globals.config.listenAddr.c_str(),
                                &listenSockAddr.sin_addr) <= 0) {
            perror("Invalid server listen address");
            exit(EXIT_FAILURE);
        }
            
        // Forcefully attaching socket to the port RAFT_PORT
        if (bind(listenSocketFd, (struct sockaddr*)&listenSockAddr,
                sizeof(listenSockAddr))
            < 0) {
            perror("bind");
            exit(EXIT_FAILURE);
        }

        if (listen(listenSocketFd, MAX_CONNECTIONS) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        printf("[Server] listening on %s:%u\n", 
                                globals.config.listenAddr.c_str(),
                                globals.config.raftPort);
        

        Raft::ListenSocket * listenSocket = 
                            new Raft::ListenSocket(listenSocketFd);
        
        registerSocket(listenSocket);
    }

    ServerSocketManager::~ServerSocketManager()
    {
    }

    void ServerSocketManager::start()
    {
        while (true) {
            struct kevent evList[MAX_EVENTS];

            /* Poll for any events oc*/
            int numEvents = kevent(kq, NULL, 0, evList, MAX_EVENTS, NULL);

            if (numEvents == -1) {
                perror("kevent failure");
                exit(EXIT_FAILURE);
            }

            else if (numEvents == 0) {
                /* No sockets are ready to accepts*/
                printf("[Server] Waiting...\n");
                continue;
            }

            printf("[Server] %d Kqueue events\n", numEvents);

            for (int i = 0; i < numEvents; i++) {

                struct kevent ev = evList[i];
                printf("[Server] Event socket is %d\n", (int) ev.ident);

                Raft::Socket *evSocket = static_cast<Raft::Socket*>(ev.udata);

                if (ev.fflags & EV_EOF) {
                    removeSocket(evSocket);
                }
                else {
                    evSocket->handleSocketEvent(ev, *this);
                }
            }
        }
    }
}