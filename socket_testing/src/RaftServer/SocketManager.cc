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

    bool SocketManager::registerSocket( uint64_t id, Socket *socket ) {

        struct kevent newEv;

        if (sockets.find(id) != sockets.end()) {
            printf("Socket with id %llu exists already\n", id);
            return false;
        }

        EV_SET(&newEv, socket->fd, EVFILT_READ, EV_ADD, 0, 0, socket);

        if (kevent(kq, &newEv, 1, NULL, 0, NULL) == -1) {
            perror("Failure to register client socket");
            return false;
        }

        sockets[id] = socket;

        printf("Registered new socket listener for socket id %llu\n", id);
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
        /* TODO: Iterate through the globals.config.clusterMap and
        and create storage entries for each of them. Most likely will have
        to be an unordered_map<uint64_t, ServerSocket *> because will need
        to have these initialized when we receive an incoming connection and
        determined it is a ServerSocket we can then add them? */

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
        

        uint64_t largestServerId = 0;
        for(auto& it : globals.config.clusterMap) {
            if (it.first > largestServerId) {
                largestServerId = it.first;
            }
        }
        Raft::ListenSocket * listenSocket = 
                            new Raft::ListenSocket(listenSocketFd,
                                                   largestServerId + 1);
        
        registerSocket(LISTEN_SOCKET_ID, listenSocket);
    }

    ServerSocketManager::~ServerSocketManager()
    {
        for (std::pair<uint64_t, Socket *> socket: sockets) {
            removeSocket(socket.second);
            delete socket.second;
        }
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
                    delete evSocket;
                }
                else {
                    evSocket->handleSocketEvent(ev, *this);
                }
            }
        }
    }
}