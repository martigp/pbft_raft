#include <string>
#include <sys/event.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <memory>
#include "RaftServer/SocketManager.hh"
#include "RaftServer/Socket.hh"
#include "Common/RPC.hh"
#include "google/protobuf/message.h"

namespace Raft {
    
    SocketManager::SocketManager( Globals& globals )
        : globals(globals),
          sockets()
    {   
        kq = kqueue();
        if (kq == -1) {
            perror("Failed to create kqueue");
            exit(EXIT_FAILURE);
        }
    }

    SocketManager::~SocketManager()
    {
        // Clean up all sockets managed by SocketManager
        for (auto& it : sockets) {
            stopSocketMonitor(it.second);
        }

        if (close(kq) == -1) {
            perror("Failed to kqueue");
            exit(EXIT_FAILURE);
        }
    }

    bool SocketManager::monitorSocket( uint64_t peerId, Socket *socket ) {

        struct kevent newEv;

        if (sockets.find(peerId) != sockets.end()) {
            printf("[SocketManager] Socket with peerid %llu already exists\n", 
            peerId);
            return false;
        }

        sockets[peerId] = socket;

        bzero(&newEv, sizeof(newEv));
        EV_SET(&newEv, socket->fd,
               EVFILT_READ, EV_ADD, 0, 0, socket);

        if (kevent(kq, &newEv, 1, NULL, 0, NULL) == -1) {
            perror("[SocketManager] Failure to register reading events on client socket\n");
            return false;
        }

        bzero(&newEv, sizeof(newEv));
        EV_SET(&newEv, socket->userEventId, 
               EVFILT_USER, EV_ADD | EV_CLEAR | EV_ENABLE, 0, 0, socket);

        if (kevent(kq, &newEv, 1, NULL, 0, NULL) == -1) {
            perror("[SocketManager] Failure to register reading events on client socket\n");
            // Probably have to remove the other event if this doesn't work!
            return false;
        }

        printf("[SocketManager] Registered new socket listener for socket id %llu\n", peerId);
        return true;
    }

    bool SocketManager::stopSocketMonitor( Socket* socket ) {
        struct kevent ev;

        /* Set flags for an event to stop the kernel from listening for
        events on this socket. */
        EV_SET(&ev, socket->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);

        if (kevent(kq, &ev, 1, NULL, 0, NULL) == -1) {
            perror("[SocketManager] Failure to remove socket fd from kqueue");
            return false;
        }

        EV_SET(&ev, socket->userEventId, EVFILT_USER, EV_DELETE, 0, 0, NULL);

        if (kevent(kq, &ev, 1, NULL, 0, NULL) == -1) {
            perror("[SocketManager] Failure to remove socket user event");
            return false;
        }

        /* Calls socket destructor which closes the socketFd.
           Last call to close will remove from kqueue so we may only have
           to call delete socket! */
        printf("[SocketManager] About to delete socket pointer with read fd %d\n", socket->fd);
        delete socket;

        return true;
    }

    void SocketManager::sendRPC( uint64_t peerId, 
                                 google::protobuf::Message& rpc,
                                 Raft::RPCType rpcType) {

        auto socketsEntry = sockets.find(peerId);
        if (socketsEntry == sockets.end()) {
            printf("Unable to find corresponding Socket to queue RPC\n");
            return;
        }

        Socket *socket = socketsEntry->second;

        Raft::RPCHeader rpcHeader(rpcType, rpc.ByteSizeLong());

        Raft::RPCPacket rpcPacket(rpcHeader, rpc);

        std::string payload;

        // Lock Acquire
        socket->sendRPCQueue.push(rpcPacket);
        // Lock Release

        struct kevent newEv;
        EV_SET(&newEv, socket->userEventId, EVFILT_USER, 0, 
               NOTE_TRIGGER, 0, socket);

        if (kevent(kq, &newEv, 1, NULL, 0, NULL) == -1) {
            printf("Failed trigger user with event");
        }

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

        printf("[SocketManager] listening on %s:%u\n", 
                                globals.config.listenAddr.c_str(),
                                globals.config.raftPort);
        

        uint64_t largestServerId = 0;
        for(auto& it : globals.config.clusterMap) {
            if (it.first > largestServerId) {
                largestServerId = it.first;
            }
        }

        Raft::ListenSocket * listenSocket = 
                new Raft::ListenSocket(listenSocketFd, globals.genUserEventId(),
                                       largestServerId + 1);
        
        monitorSocket(LISTEN_SOCKET_ID, listenSocket);
    }

    ServerSocketManager::~ServerSocketManager()
    {
    }

    void ServerSocketManager::start()
    {   
        int numLoops = 0;
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
                printf("[SocketManager] Waiting...\n");
                continue;
            }

            printf("[SocketManager] Received %d socket events\n", numEvents);

            for (int i = 0; i < numEvents; i++) {

                struct kevent ev = evList[i];
                printf("[SocketManager] Event on socket %lu, iteration %d \n", ev.ident, numLoops++);

                if (numLoops > 5) {
                    goto exit;
                }

                Raft::Socket *evSocket = static_cast<Raft::Socket*>(ev.udata);

                if (ev.fflags & EV_EOF) {
                    stopSocketMonitor(evSocket);
                } else if (ev.fflags & EV_ERROR) {
                    perror("kevent error");
                    exit(EXIT_FAILURE);
                }
                else {
                    bool success = evSocket->handleSocketEvent(ev, *this);
                    printf("[ServerSocket] Left Socket handler\n");
                    if (!success && ev.ident != LISTEN_SOCKET_ID)
                    {
                        stopSocketMonitor(evSocket);
                    }
                }
            }
        }
        exit:
            printf("[SocketManager] exited event loop");
    }
}