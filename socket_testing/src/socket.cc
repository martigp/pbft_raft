#include <memory>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/event.h>
#include "RaftGlobals.hh"
#include "Socket.hh"
#include "SocketManager.hh"
#include "protobuf/test.pb.h"

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

            bytesRead = recv(evSocketFd, buf, sizeof(buf), 0);

            Test::TestMessage receivedProtoMsg;

            receivedProtoMsg.ParseFromArray(buf, sizeof(buf));

            printf("[Server] Bytes Received %zu\n"
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

    ListenSocket::ListenSocket( int fd )
        : Socket(fd)
    { }

    ListenSocket::~ListenSocket()
    { }

    void ListenSocket::handleSocketEvent( struct kevent& ev,
                                          SocketManager& socketManager) {
        // struct sockaddr clientAddr;
        // socklen_t addrLen;
        // TODO: Use these to determine what type of connection it is e.g.
        // RaftServer v.s. RaftClient by cross referencing addresses with
        // config addresses of RaftServers
        // Currently assumes only RaftServers

        int socketFd = accept(int (ev.ident), NULL, NULL);
        if (socketFd < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("[Server] accepted new client on socket %d\n", socketFd);

        ServerSocket * serverSocket = new ServerSocket(socketFd);

        printf("Successfully constructed server socket\n");

        socketManager.registerEv(serverSocket);

        printf("Successfully set events on newEv\n");
    }
}