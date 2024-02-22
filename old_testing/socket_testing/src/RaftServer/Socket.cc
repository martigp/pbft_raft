#include <stdio.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>

#include "Common/RPC.hh"
#include "Protobuf/RaftRPC.pb.h"
#include "Protobuf/test.pb.h"
#include "RaftServer/RaftGlobals.hh"
#include "RaftServer/Socket.hh"
#include "RaftServer/SocketManager.hh"

namespace Raft {

Socket::ReceivedMessage::ReceivedMessage(size_t initialBufferSize)
    : numBytesRead(0),
      bufferedBytes(new char[initialBufferSize]),
      bufLen(initialBufferSize),
      rpcType(Raft::RPCType::NONE) {}

Socket::ReceivedMessage::~ReceivedMessage() { delete[] bufferedBytes; }

Socket::Socket(int fd, uint32_t userEventId, uint64_t peerId,
               PeerType peerType, SocketManager& socketManager)
    : fd(fd),
      userEventId(userEventId),
      peerId(peerId),
      peerType(peerType),
      receivedMessage(RPC_HEADER_SIZE),
      sendRPCQueue(),
      socketManager(socketManager) {
    printf("[Socket] Constructed new socket with fd %d, userEventId %u\n", fd,
           userEventId);
}

Socket::~Socket() {
    if (close(fd) != -1) {
        perror("Failed to close socket");
        exit(EXIT_FAILURE);
    }

    printf("[Socket] Socket object with fd %d deleted.", fd);
}

void
Socket::handleReceiveEvent(int64_t data) {
    int64_t totalBytesRead = 0;

    // Breaks when no more bytes to be read or error.
    while (totalBytesRead < data) {
        printf("[Socket] New loop bytes read %lld, bytes received %lld\n",
                                                        totalBytesRead, data);
        // We have not read in a full header yet. Attempt to read in the
        // remaining bytes needed for a complete header so that we know the
        // type of RPC sent and the size of the RPC.
        if (receivedMessage.numBytesRead < RPC_HEADER_SIZE) {
            size_t bytesAvailable = 
                receivedMessage.bufLen - receivedMessage.numBytesRead;

            // Read into first unread byte in buffer whose size is set to the
            // size of an RPCHeader.
            ssize_t bytesRead =
                recv(fd, receivedMessage.bufferedBytes + receivedMessage.numBytesRead,
                     bytesAvailable, MSG_DONTWAIT);

            printf("Bytes Read %zu\n", bytesRead);

            if (bytesRead == -1 ) {
                perror("[Socket] Error reading header\n");
                disconnect();
                break;
            } else if (bytesRead == 0)
                break;

            receivedMessage.numBytesRead += bytesRead;
            totalBytesRead += bytesRead;

            // If successfully read in the entire header, read the RPC type
            // and length of RPC fields from header
            if (receivedMessage.numBytesRead == RPC_HEADER_SIZE) {
                printf("[ServerSocket] Received full header\n");

                Raft::RPCHeader header(receivedMessage.bufferedBytes);

                // Re-initialize the socket read buffer to the size of the
                // RPC.
                delete[] receivedMessage.bufferedBytes;
                receivedMessage.bufferedBytes = new char[header.payloadLength];
                bzero(receivedMessage.bufferedBytes, header.payloadLength);

                receivedMessage.bufLen = header.payloadLength;

                receivedMessage.rpcType = header.rpcType;
            }
        }

        // Header has been read, all bytes in the buffer are payload bytes
        // and reads we do will append to that.
        if (receivedMessage.numBytesRead >= RPC_HEADER_SIZE) {
            size_t bytesOfPayloadRead =
                receivedMessage.numBytesRead - RPC_HEADER_SIZE;

            size_t bytesAvailable = receivedMessage.bufLen - bytesOfPayloadRead;

            ssize_t bytesRead = 
                recv(fd, receivedMessage.bufferedBytes + bytesOfPayloadRead,
                     bytesAvailable, MSG_DONTWAIT);

            if (bytesRead == -1) {
                perror("[Server Socket] Error reading payload");
                disconnect();
                return;
            } else if (bytesRead == 0)
                break;

            receivedMessage.numBytesRead += bytesRead;
            totalBytesRead += bytesRead;

            if (receivedMessage.numBytesRead == RPC_HEADER_SIZE + receivedMessage.bufLen) {
                printf("[ServerSocket] Received network packet\n");
                if (peerType == PeerType::RAFT_CLIENT) {
                    // ConsensusUnit->
                    // serverSocketManager.globals.handleClientReq
                    // args: (uint64_t peerId, Raft::RPCHeader head, char *payload)
                } else {
                    // serverSocketManager.globals.handleServerReq
                    //  args: id, opcode, std::move(receivedMessage.bufferedBytes)
                }

                // To Remove, used for testing.
                Raft::RPC::StateMachineCmd::Request rpc;
                rpc.ParseFromArray(receivedMessage.bufferedBytes, receivedMessage.bufLen);
                printf("Client RPC has command %s\n", rpc.cmd().c_str());
                socketManager.sendRPC(peerId, rpc,
                                      Raft::RPCType::STATE_MACHINE_CMD);
                // Reset read bytes
                receivedMessage = ReceivedMessage(RPC_HEADER_SIZE);
            }
        }
    }
}

void
Socket::checkConnection() {
    // do nothing
}

void
Socket::disconnect() {
    socketManager.stopSocketMonitor(this);
}

ClientSocket::ClientSocket(int fd, uint32_t userEventId, uint64_t peerId,
                           SocketManager& socketManager,
                           struct sockaddr_in peerAddress)
    : Socket(fd, userEventId, peerId, PeerType::RAFT_CLIENT, socketManager),
      eventLock(),
      eventCv(),
      killThreadCv(),
      peerAddress(peerAddress),
      killThread(false),
      threadKilled(false)
{
}

ClientSocket::~ClientSocket() {
    eventLock.lock();
    killThread = true;
    while (!threadKilled)
        killThreadCv.wait(eventLock);
    eventLock.unlock();
}

void
ClientSocket::handleUserEvent(){
    eventLock.lock();
    eventCv.notify_all();
    eventLock.unlock();
}


void clientSocketMain(void *args) {
    
    ClientSocket *clientSocket = (ClientSocket *)args;

    printf("[ClientSocket] Attempting to connect to Raft Server %llu\n",
                                                        clientSocket->peerId);
    if (connect(clientSocket->fd, 
                (struct sockaddr *)&(clientSocket->peerAddress), 
                sizeof(clientSocket->peerAddress)) < 0) {
        clientSocket->disconnect();
    }

    while (true) {
        clientSocket->eventLock.lock();
        if (clientSocket->killThread) {
            break;
        }
        else if (!clientSocket->sendRPCQueue.empty()) {
            Raft::RPCPacket rpcPacket = clientSocket->sendRPCQueue.front();
            clientSocket->eventLock.unlock();

            size_t payloadLen = rpcPacket.header.payloadLength;

            char buf[RPC_HEADER_SIZE + payloadLen];

            // Might have to do some clever network ordering before sending
            rpcPacket.header.SerializeToArray(buf, RPC_HEADER_SIZE);
            memcpy(buf + RPC_HEADER_SIZE, rpcPacket.payload, payloadLen);

            if (send(clientSocket->fd, buf, sizeof(buf), 0) == -1) {

                perror("Failure to send request to RaftServer");
                if (errno == ECONNRESET) {
                    // Might be a race condition here with the closing
                    printf("[ClientSocket] Connection Reset\n");
                    // Does not have the lock
                    clientSocket->disconnect();
                    clientSocket->eventLock.lock();
                    break;
                } else {
                    exit(EXIT_FAILURE);
                }
            }
        } else {
            // No RPCs to send, wait.
            clientSocket->eventCv.wait(clientSocket->eventLock);
        }
    }

    clientSocket->threadKilled = true;
    clientSocket->killThreadCv.notify_all();
    clientSocket->eventLock.unlock();
}


ServerSocket::ServerSocket(int fd, uint32_t userEventId, uint64_t peerId,
                           PeerType peerType, SocketManager& socketManager)
    : Socket(fd, userEventId, peerId, peerType, socketManager) {}

ServerSocket::~ServerSocket() {
    printf("[ServerSocket] Successfully deleted server socket\n");
}

void
ServerSocket::handleUserEvent() {
    printf("[Socket] Entering user event handling\n");

    // Lock Acquire
    while (!sendRPCQueue.empty()) {
        // TODO: Might have to do a move here, unclear on memory ops.
        Raft::RPCPacket rpcPacket = sendRPCQueue.front();
        sendRPCQueue.pop();
        // Lock Release

        size_t payloadLen = rpcPacket.header.payloadLength;

        char buf[RPC_HEADER_SIZE + payloadLen];

        // Might have to do some clever network ordering before sending
        rpcPacket.header.SerializeToArray(buf, RPC_HEADER_SIZE);
        memcpy(buf + RPC_HEADER_SIZE, rpcPacket.payload, payloadLen);

        if (send(fd, buf, sizeof(buf), 0) == -1) {
            perror("Failure to send to client");
            disconnect();
        }
        printf("[Server Socket] Should sent response\n");
        // Lock Acquire
    }
    // Lock Release
}

ListenSocket::ListenSocket(int fd, uint32_t userEventId,
                           SocketManager& socketManager,
                           uint64_t firstRaftClientId)
    : Socket(fd, userEventId, LISTEN_SOCKET_ID, PeerType::NONE, socketManager),
      nextRaftClientId(firstRaftClientId) {}

ListenSocket::~ListenSocket() {}

void
ListenSocket::handleReceiveEvent(int64_t data) {

    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen;

    printf("[ListenSocket] Number of connections attempted %lld\n", data);

    int socketFd =
        accept(fd, (struct sockaddr *)&clientAddr, &clientAddrLen);
    
    if (socketFd < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("[Server] accepted new client on socket %d\n", socketFd);

    uint64_t newPeerId = 0;
    ServerSocket::PeerType peerType;
    // Check if incoming connection is from a RaftClient or RaftServer
    for (auto &it : socketManager.globals.config.clusterMap) {
        if (ntohl(it.second.sin_addr.s_addr) ==
            ntohl(clientAddr.sin_addr.s_addr)) {
            printf("[ListenSocket] Accepted connection request from RaftServer "
                   "with id %llu\n", it.first);
            
            newPeerId = it.first;
            peerType = ServerSocket::PeerType::RAFT_SERVER;
            break;
        }
    }

    // Connection is a RaftClient, assign it a peerId
    if (newPeerId == 0) {
        newPeerId = nextRaftClientId;
        nextRaftClientId++;

        peerType = ServerSocket::PeerType::RAFT_CLIENT;
        printf(
            "[ListenSocket] connection request from RaftClient with id"
            " %llu\n",
            newPeerId);
    }

    ServerSocket *serverSocket = new ServerSocket(
        socketFd, socketManager.globals.genUserEventId(), newPeerId, peerType,
        socketManager);

    socketManager.monitorSocket(serverSocket);
}

void
ListenSocket::handleUserEvent() {
    printf("[ListenSocket] Error no user events on Listen Sockets\n");
    exit(EXIT_FAILURE);
}

}  // namespace Raft