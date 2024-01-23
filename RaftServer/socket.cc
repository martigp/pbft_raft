#include <memory>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/event.h>
#include "SocketManager.hh"
#include "socket.hh"

namespace Raft {

    Socket::Socket(int fd, SocketManager* socketManager)
        : fd(fd)
        , socketManager(socketManager)
    { }

    Socket::~Socket()
    {
        if (close(fd) != -1) {
            perror("Failed to close socket");
            exit(EXIT_FAILURE);
        }
        printf("[Server] socket %d deleted.", fd);
    }

    ServerSocket::ServerSocket(int fd, SocketManager* socketManager)
        : Socket(fd, socketManager)
    {

    }

    ServerSocket::~ServerSocket()
    { }

    void ServerSocket::handleSocketEvent(struct kevent& ev) {
        int evSocketFd = (int)ev.ident;

        if (ev.filter & EVFILT_READ && (int) ev.data > 0) {
            ssize_t bytesRead;
            char clientReadBuffer[256];
            char response[256];

            memset(clientReadBuffer, 0, sizeof(clientReadBuffer));

            bytesRead = recv(evSocketFd, clientReadBuffer,
                             sizeof(clientReadBuffer), 0);

            printf("[Server] Bytes Received %zu\n"
                    "[Server] Client %d sent message: %s\n",
                                    bytesRead, evSocketFd, clientReadBuffer);
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

    ListenSocket::ListenSocket(int fd, SocketManager* socketManager)
        : Socket(fd, socketManager)
    { }

    ListenSocket::~ListenSocket()
    { }

    void ListenSocket::handleSocketEvent(struct kevent& ev) {
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

        ServerSocket * serverSocket = new ServerSocket(socketFd, socketManager);
        socketManager->addkQueueSocket(serverSocket);
    }
}