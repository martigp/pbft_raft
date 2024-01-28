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

Socket::ReadBytes::ReadBytes(size_t initialBufferSize)
    : numBytesRead(0),
      bufferedBytes(new char[initialBufferSize]),
      bufLen(initialBufferSize),
      rpcType(Raft::RPCType::NONE) {}

Socket::ReadBytes::~ReadBytes() { delete[] bufferedBytes; }

Socket::Socket(int fd, uint32_t userEventId)
    : fd(fd),
      userEventId(userEventId),
      readBytes(RPC_HEADER_SIZE),
      sendRPCQueue() {
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

ServerSocket::ServerSocket(int fd, uint32_t userEventId, uint64_t peerId,
                           PeerType peerType)
    : Socket(fd, userEventId), peerId(peerId), peerType(peerType) {}

ServerSocket::~ServerSocket() {}

bool
ServerSocket::handleReadEvent(SocketManager &socketManager) {
    ssize_t bytesRead;

    // Breaks when no more bytes to be read or error.
    while (true) {
        // We have not read in a full header yet. Attempt to read in the
        // remaining bytes needed for a complete header so that we know the
        // type of RPC sent and the size of the RPC.
        if (readBytes.numBytesRead < RPC_HEADER_SIZE) {
            size_t bytesAvailable = readBytes.bufLen - readBytes.numBytesRead;

            // Read into first unread byte in buffer whose size is set to the
            // size of an RPCHeader.
            bytesRead =
                recv(fd, readBytes.bufferedBytes + readBytes.numBytesRead,
                     bytesAvailable, MSG_DONTWAIT);

            printf("Bytes Read %zu\n", bytesRead);

            if (bytesRead == -1) {
                printf("[Server Socket] Error reading header\n");
                return false;
            } else if (bytesRead == 0)
                break;

            readBytes.numBytesRead += bytesRead;

            // If successfully read in the entire header, read the RPC type
            // and length of RPC fields from header
            if (readBytes.numBytesRead == RPC_HEADER_SIZE) {
                printf("[ServerSocket] Received full header\n");

                Raft::RPCHeader header(readBytes.bufferedBytes);

                // Re-initialize the socket read buffer to the size of the
                // RPC.
                delete[] readBytes.bufferedBytes;
                readBytes.bufferedBytes = new char[header.payloadLength];
                bzero(readBytes.bufferedBytes, header.payloadLength);

                readBytes.bufLen = header.payloadLength;

                readBytes.rpcType = header.rpcType;
            }
        }

        // Header has been read, all bytes in the buffer are payload bytes
        // and reads we do will append to that.
        if (readBytes.numBytesRead >= RPC_HEADER_SIZE) {
            size_t bytesOfPayloadRead =
                readBytes.numBytesRead - RPC_HEADER_SIZE;

            size_t bytesAvailable = readBytes.bufLen - bytesOfPayloadRead;

            bytesRead = recv(fd, readBytes.bufferedBytes + bytesOfPayloadRead,
                             bytesAvailable, MSG_DONTWAIT);

            if (bytesRead == -1) {
                perror("[Server Socket] Error reading payload");
                return false;
            } else if (bytesRead == 0)
                break;

            readBytes.numBytesRead += bytesRead;

            if (readBytes.numBytesRead == RPC_HEADER_SIZE + readBytes.bufLen) {
                printf("[ServerSocket] Received network packet\n");
                if (peerType == PeerType::RAFT_CLIENT) {
                    // ConsensusUnit->
                    // serverSocketManager.globals.handleClientReq
                    // args: id, opcode, std::move(readBytes.bufferedBytes)
                } else {
                    // serverSocketManager.globals.handleServerReq
                    //  args: id, opcode, std::move(readBytes.bufferedBytes)
                }

                // To Remove, used for testing.
                Raft::RPC::StateMachineCmd::Request rpc;
                rpc.ParseFromArray(readBytes.bufferedBytes, readBytes.bufLen);
                printf("Client RPC has command %s\n", rpc.cmd().c_str());
                socketManager.sendRPC(peerId, rpc,
                                      Raft::RPCType::STATE_MACHINE_CMD);
                // Reset read bytes
                readBytes = ReadBytes(RPC_HEADER_SIZE);
            }
        }
    }
    return true;
}

bool
ServerSocket::handleUserEvent() {
    printf("[Socket] Entering user event handling\n");

    // Lock Acquire
    while (!sendRPCQueue.empty()) {
        // TODO: Might have to do a move here, unclear on memory ops.
        Raft::RPCPacket rpcPacket = sendRPCQueue.front();
        sendRPCQueue.pop();

        size_t payloadLen = rpcPacket.header.payloadLength;

        char buf[RPC_HEADER_SIZE + payloadLen];

        // Lock Release
        // Might have to do some clever network ordering before sending
        rpcPacket.header.SerializeToArray(buf, RPC_HEADER_SIZE);
        memcpy(buf + RPC_HEADER_SIZE, rpcPacket.payload, payloadLen);

        if (send(fd, buf, sizeof(buf), 0) == -1) {
            perror("Failure to send to client");
            return false;
        }
        printf("[Server Socket] Should sent response\n");
        // Lock Acquire
    }
    // Lock Release
    return true;
}

bool
ServerSocket::handleSocketEvent(struct kevent &ev,
                                SocketManager &socketManager) {
    printf("[ServerSocket] Handling event id %lu\n", ev.ident);

    // Received bytes from the Peer
    if (ev.filter & EVFILT_READ && (int)ev.ident == fd) {
        handleReadEvent(socketManager);
    }

    // User Triggered event, data to write to network.
    else if (ev.filter & EVFILT_USER) {
        handleUserEvent();
    } else {
        printf("Server does not know what happened %u\n", ev.fflags);
    }

    return true;
}

ListenSocket::ListenSocket(int fd, uint32_t userEventId,
                           uint64_t firstRaftClientId)
    : Socket(fd, userEventId), nextRaftClientId(firstRaftClientId) {}

ListenSocket::~ListenSocket() {}

bool
ListenSocket::handleReadEvent(SocketManager &socketManager) {
    return true;
}

bool
ListenSocket::handleUserEvent() {
    return true;
}

bool
ListenSocket::handleSocketEvent(struct kevent &ev,
                                SocketManager &socketManager) {
    handleReadEvent(socketManager);

    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen;
    // TODO: Use these to determine what type of connection it is e.g.
    // RaftServer v.s. RaftClient by cross referencing addresses with
    // config addresses of RaftServers
    // Currently assumes only RaftServers

    int socketFd =
        accept(int(ev.ident), (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (socketFd < 0) {
        perror("accept");
        return false;
    }

    printf("[Server] accepted new client on socket %d\n", socketFd);

    uint64_t newPeerId = 0;
    ServerSocket::PeerType peerType;
    // Check if incoming connection is from a RaftClient or RaftServer
    for (auto &it : socketManager.globals.config.clusterMap) {
        if (ntohl(it.second.sin_addr.s_addr) ==
            ntohl(clientAddr.sin_addr.s_addr)) {
            // TODO: mark socket RaftServer/RaftClient upon receipt
            // This is going to help for when processing an RPC?
            // Might just be an enum in the ServerSocket object?
            printf(
                "[ListenSocket] Accepted connection request from RaftServer "
                "with"
                "id %llu\n",
                it.first);
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
        socketFd, socketManager.globals.genUserEventId(), newPeerId, peerType);

    socketManager.monitorSocket(newPeerId, serverSocket);

    return true;
}
}  // namespace Raft