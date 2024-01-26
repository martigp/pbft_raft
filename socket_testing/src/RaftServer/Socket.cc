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
    
    Socket::ReadBytes::ReadBytes(size_t numBytes)
        : numBytes (0)
    {
        bufferedBytes = new char[numBytes];
    }

    Socket::ReadBytes::~ReadBytes() {
        delete bufferedBytes;
    }


    Socket::Socket( int fd, uint32_t userEventId )
        : fd(fd),
          userEventId (userEventId),
          readBytes(sizeof(Test::TestMessage)),
          sendRPCQueue()
    { 
        printf("[Socket] Constructed new socket with fd %d, userEventId %u\n", 
                                                              fd, userEventId);
    }

    Socket::~Socket()
    {
        if (close(fd) != -1) {
            perror("Failed to close socket");
            exit(EXIT_FAILURE);
        }

        printf("[Socket] Socket object with fd %d deleted.", fd);
    }

    void Socket::sendRPC(Test::TestMessage msg) {
        sendRPCQueue.push(msg);
    }

    ServerSocket::ServerSocket( int fd, uint32_t userEventId )
        : Socket(fd, userEventId)
    {

    }

    ServerSocket::~ServerSocket()
    { }

    void ServerSocket::handleSocketEvent( struct kevent& ev,
                                          SocketManager& socketManager ) {

        printf("[ServerSocket] Handling event id %lu\n", ev.ident);
        // Check if user triggered event on the write socket
        if (ev.filter & EVFILT_READ && (int) ev.ident == fd) {
            printf("[Socket] Entering READ event handling\n");
            ssize_t bytesRead;
            char *readBuf;

            size_t bytesAvailable = 
                    sizeof(readBytes.bufferedBytes) - readBytes.numBytes;
            
            readBuf = readBytes.bufferedBytes + readBytes.numBytes;

            printf("[Socket] Bytes Received: %ld\n", ev.data);

            bytesRead = recv(fd, readBuf, bytesAvailable, 0);
            printf("[Socket] Bytes Read %zu\n", bytesRead);

            readBytes.numBytes += bytesRead;


            if (readBytes.numBytes == sizeof(Test::TestMessage))
            {
                Test::TestMessage receivedProtoMsg;
                receivedProtoMsg.ParseFromArray(readBytes.bufferedBytes,
                                                sizeof(Test::TestMessage));

                printf("[Socket] Client with id %llu sent message: %s\n",
                            receivedProtoMsg.sender(),
                            receivedProtoMsg.msg().c_str());

                socketManager.sendRPC(this, receivedProtoMsg);
                bzero(readBytes.bufferedBytes, sizeof(Test::TestMessage));
                readBytes.numBytes = 0;
            }
        }
        else if (ev.filter & EVFILT_USER) {
            printf("[Socket] Entering user event handling\n");            
            char response[sizeof(Test::TestMessage)];

            while(!sendRPCQueue.empty()) {
                bzero(response, sizeof(response));

                Test::TestMessage protoMsg = sendRPCQueue.front();
                sendRPCQueue.pop();


	            protoMsg.SerializeToArray(response, sizeof(response));

                if (send(fd, response, sizeof(response), 0) == -1) {
                    perror("Failure to send to client");
                    exit(EXIT_FAILURE);
                }
                printf("[Server Socket] Should sent response\n");
            }
        }
        else {
            printf("Server does not know what happened %u\n", ev.fflags);
        }
    }

    ListenSocket::ListenSocket( int fd, uint32_t userEventId,
                                uint64_t firstRaftClientId )
        : Socket(fd, userEventId),
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
                    printf("[ListenSocket] Accepted connection request from RaftServer with"
                           "id %llu\n", it.first);
                    clientId = it.first;
            }
            else
            {
                clientId = nextRaftClientId;
                nextRaftClientId++;
                printf("[ListenSocket] connection request from RaftClient with id"
                       " %llu\n", clientId);
            }
        };

        ServerSocket * serverSocket = 
                new ServerSocket(socketFd,
                                 socketManager.globals.genUserEventId());

        socketManager.monitorSocket(clientId, serverSocket);
    }
}