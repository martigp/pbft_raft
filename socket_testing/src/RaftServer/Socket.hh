#ifndef RAFT_SOCKET_H
#define RAFT_SOCKET_H

#include <queue>
#include <sys/event.h>
#include <mutex>
#include <condition_variable>
#include "RaftServer/RaftGlobals.hh"
#include "Protobuf/RaftRPC.pb.h"
#include "Common/RPC.hh"

namespace Raft {

class Globals;
class SocketManager;
class ClientSocketManager;


/**
 * @brief Sockets are attached to a socket file descriptor when they are
 * registered to the kernel for monitoring. When the kernel alerts the user
 * of an event on the socket file descriptor the Socket is called to handle
 * the event with custom event handlers based on the type of event.
 */
class Socket {
    
    friend SocketManager;

    public:

        enum PeerType {
            NONE,
            RAFT_CLIENT,
            RAFT_SERVER
        };

        /**
         * @brief Construct a new Socket object. Once constructed it is
         * registered with the corresponding socket file descriptor to the
         * kernel to respond to events on the socket.
         * 
         */
        Socket( int fd, uint32_t userEventId, uint64_t peerId,
                PeerType peerType, SocketManager& socketManager );

        /* Destructor */
        virtual ~Socket() = 0;

        /**
         * @brief This method is potentially by a subclass to handle the event
         * triggered when a peer has sent data to the socket. The socket
         * will read the data and act accordingly. Default it assuming this is
         * a connected socket and these are bytes of an RPC
         * 
         * @param data The EVFILT_READ data value provided by kernel. This is
         * the bytes of data available to read.
         * 
         * @return Returns false if any errors occured when reading from the
         * socket. True if no errors.
         * 
         * TODO: Remove Socket manager when done with testing.
         */
        virtual void handleReceiveEvent(int64_t data);

        /**
         * @brief This method is overriden by a subclass to handle the event
         * triggered by another Raft Module (user) when a Raft Module has added
         * RPC to the sockets RPC queue. The Socket will attempt to write these
         * to its peer.
         * 
         * @return Whether any errors occured when trying to write to the peer.
         */
        virtual void handleUserEvent() = 0;

        virtual void checkConnection();

        /**
         * @brief Handles errors that don't require a crash by disconnecting 
         * the conenction on the socket. The default behaviour is to clean up
         * everything to do with Client Socket.
         */
        void disconnect();

    protected:
        /**
         * @brief The socket file descriptor.
         */
        int fd;

        /**
         * @brief Unique user event id used by Raft to signal to kernel that
         * a user triggered an event on the socket.
         * 
         */
        const uint32_t userEventId;

        /**
         * @brief The RaftServer Id, used by the socket manager to
         * identify which Client Socket to send RPCs on.
         */
        uint64_t peerId;

        /**
         * @brief Whether the peer is a RaftClient or a RaftServer. Used to
         * determine which Consensus Module API to call to handle an RPC.
         */
        [[maybe_unused]] PeerType peerType;

        /**
         * @brief Wrapper that stores the bytes read from socket buffer and
         * the number of bytes read from socket buffer.
         * 
         */
        class ReceivedMessage {
            public:
                /**
                 * @brief Constructor
                 * 
                 * @param headerSize Initial size of buffer. This is
                 * set to the size of a header.
                 */
                ReceivedMessage(size_t headerSize);

                ~ReceivedMessage();

                /**
                 * @brief Number of bytes read in from socket buffer.
                 * 
                 */
                size_t numBytesRead;
                /**
                 * @brief Bytes buffered from reading from socket. Will contain
                 * exclusively RPC header bytes, switching when numBytesRead is
                 * equal to the Header size.
                 */
                char *bufferedBytes;

                /**
                 * @brief The size of the bufferedBytes Buffer.
                 * 
                 */
                size_t bufLen;

                /**
                 * @brief Header parsed from incoming bytes. Every RPC is 
                 * prepended by a header.
                 */
                Raft::RPCType rpcType;
        };

        ReceivedMessage receivedMessage;

        /**
         * @brief Queue of RPCs to send to peer. This queue is checked when
         * Raft triggers a user event on the socket.
         */
        std::queue< Raft::RPCPacket> sendRPCQueue;

        /**
         * @brief The socket manager responsible for the socket. This is used
         * for error handling when the socket needs to close down.
         */
        SocketManager& socketManager;


}; // class Socket

/**
 * @brief A socket that listens for incoming connection requests on a Raft
 * Server and produces connections for the client to send Raft RPC requests.
 * 
 */
class ListenSocket : public Socket {
    public:
        /**
         * @brief Construct a new Listen Socket object that listens for and
         * handles incoming connection requests.
         */
        ListenSocket( int fd, uint32_t userEventId, 
                      SocketManager& socketManager,
                      uint64_t firstRaftClientId);

        /* Destructor */
        ~ListenSocket();

        /**
         * @brief Method called whenever the kernel notifies us of an attempt to
         * connect to the Raft Server. If valid it will produce a ServerSocket
         * and add to the list of sockets that the kernel monitors. Otherwise 
         * fails silently.
         * 
         * @param data The filter data value provided by the kqueue.
         */
        void handleReceiveEvent(int64_t data);

        /**
         * @brief Unused right now. Filler to pass compiler checks that all
         * pure virtual methods implemented.
         * 
         * @param socketManager 
         * @return true 
         * @return false 
         */
        void handleUserEvent();

    private:
        /**
         * @brief The Id to be assigned to the next RaftClient that connects
         * to this RaftServer.
         */
        uint64_t nextRaftClientId;
}; // class ListenSocket

/**
 * @brief A socket connected to a Raft Server for sending RPC requests.
 */
class ClientSocket : public Socket {

    friend ClientSocketManager;

    friend void clientSocketMain(void *args);

    public:
        /**
         * @brief Constructor
         */
        ClientSocket( int fd, uint32_t userEventId, uint64_t peerId, 
                      SocketManager& socketManager,
                      struct sockaddr_in peerAddress);

        ~ClientSocket();

        /**
         * @brief Signals to the thread responsible for user events to wakeup
         * to process the event.
         */
        void handleUserEvent();

    protected:

        /**
         * @brief Lock associated with the condition variable for signalling
         * a change in state of the ClientSocket.
         * 
         */
        std::mutex eventLock;

        /**
         * @brief Condition variable to signal to thread associated with the
         * client socket that either an RPC has been added to its queue to send
         * OR that the connection has gone bad and has to be restarted.
         */
        std::condition_variable_any eventCv;

        /**
         * @brief Condition variable for ClientSocketManager to wait on for
         * the client socket's thread that it is done.
         */
        std::condition_variable_any killThreadCv;

        struct sockaddr_in peerAddress;

        /**
         * @brief Flag to indicate to client socket thread to shut down.
         */
        bool killThread;

        bool threadKilled;

}; // class ClientSocket

/**
 * @brief 
 * 
 * @param clientSocket 
 */
void clientSocketMain(void *args);

/**
 * @brief A socket that listens for incoming Raft RPC requests.
 * ServerSockets are sockets for connections where the owner is server of the
 * underlying connection. The corresponding Client can be a RaftClient or a
 * RaftServer.
 */
class ServerSocket : public Socket {
    public:

         /**
         * @brief Construct a new Server Socket object that handles requests
         * from a socket connected to a client and sends responses.
         */
        ServerSocket( int fd , uint32_t userEventId, uint64_t peerId, 
                      PeerType peerType, SocketManager& socketManager );

        /* Destructor */
        ~ServerSocket();

        /**
         * @brief Method called when another RaftModule produced and RPC and 
         * signalled the socket to send it. The socket will attempt to send 
         * every RPC in its queue (populated by other Raft Modules) to its peer.
         * 
         * @return Whether the socket should be destroyed as a result of an
         * error occuring while trying to send an RPC.
         */
        void handleUserEvent();
}; // class ServerSocket

} // namespace Raft

#endif /* RAFT_SOCKET_H */