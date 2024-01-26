#include <memory>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/event.h>
#include "RaftServer/RaftGlobals.hh"
#include "RaftServer/Socket.hh"
#include "RaftServer/SocketManager.hh"
#include "Protobuf/test.pb.h"

namespace Raft {

    Socket::Socket( int fd )
        : fd(fd)
    { }

    Socket::~Socket()
    {
        if (close(fd) != -1) {
            perror("Failed to close socket");
            exit(EXIT_FAILURE);
        }
        printf("[Server] socket %d deleted.", fd);
    }

    ServerSocket::ServerSocket( int fd )
        : Socket(fd)
    {

    }

    ServerSocket::~ServerSocket()
    { }

    void ServerSocket::handleSocketEvent( struct kevent& ev,
                                          SocketManager& socketManager ) {
        int evSocketFd = (int)ev.ident;

        if (ev.filter & EVFILT_READ && (int) ev.data > 0) {
            ssize_t bytesRead;
            char buf[sizeof(Test::TestMessage)];
            char response[256];

            memset(buf, 0, sizeof(buf));

            printf("[Server] Bytes Received: %ld", ev.data);

            bytesRead = recv(evSocketFd, buf, sizeof(Test::TestMessage), 0);

            Test::TestMessage receivedProtoMsg;
            receivedProtoMsg.ParseFromArray(buf, sizeof(Test::TestMessage));

            printf("[Server] Bytes Read %zu\n"
                    "[Server] Client with id %llu sent message: %s\n",
                                bytesRead,
                                receivedProtoMsg.sender(),
                                receivedProtoMsg.msg().c_str());
            
            memset(response, 0, sizeof(response));
            strncpy(response, "Received your message", sizeof(response));

            if (send(evSocketFd, response, sizeof(response), 0) == -1) {
                perror("Failure to send to client");
                exit(EXIT_FAILURE);
            }
        }
        else {
            printf("Server does not know what happened %u\n", ev.fflags);
        }
    }

    ListenSocket::ListenSocket( int fd, uint64_t firstRaftClientId )
        : Socket(fd),
          nextRaftClientId(firstRaftClientId)
    { }

    ListenSocket::~ListenSocket()
    { }

    void ListenSocket::handleSocketEvent( struct kevent& ev,
                                          SocketManager& socketManager) {
        
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen;
        // TODO: Use these to determine what type of connection it is e.g.
        // RaftServer v.s. RaftClient by cross referencing addresses with
        // config addresses of RaftServers
        // Currently assumes only RaftServers

        int socketFd = accept(int(ev.ident), (struct sockaddr *) &clientAddr,
                              &clientAddrLen);
        if (socketFd < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("[Server] accepted new client on socket %d\n", socketFd);

        uint64_t clientId;

        // Check if incoming connection is from a RaftClient or RaftServer
        for (auto& it : socketManager.globals.config.clusterMap) {
            if (ntohl(it.second.sin_addr.s_addr) == 
                ntohl(clientAddr.sin_addr.s_addr)) {
                    // TODO: mark socket RaftServer/RaftClient upon receipt
                    // This is going to help for when processing an RPC?
                    // Might just be an enum in the ServerSocket object?
                    printf("Accepted connection request from RaftServer with"
                           "id %llu\n", it.first);
                    clientId = it.first;
            }
            else
            {
                clientId = nextRaftClientId;
                nextRaftClientId++;
                printf("Accepted connection request from RaftClient with id"
                       "%llu\n", clientId);
            }
        };

        ServerSocket * serverSocket = new ServerSocket(socketFd);

        printf("Successfully constructed server socket\n");

        socketManager.registerSocket(clientId, serverSocket);
    }
}