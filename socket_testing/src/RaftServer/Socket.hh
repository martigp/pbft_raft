#ifndef RAFT_SOCKET_H
#define RAFT_SOCKET_H

#include <queue>
#include <sys/event.h>
#include "RaftServer/RaftGlobals.hh"
#include "Protobuf/test.pb.h"

namespace Raft {

/**
 * @brief A Socket is registered to an event loop and its method
 * handleSocketEvent() is called whenever an event occurs on the
 * socket.
 */

class Globals;
class SocketManager;

/**
 * @brief Sockets are attached to a socket file descriptor when they are
 * registered to the kernel for monitoring. When the kernel alerts the user
 * of an event on the socket file descriptor the Socket is called to handle
 * the event.
 */
class Socket {
    
    friend SocketManager;

    public:
        /**
         * @brief Construct a new Socket object. Once constructed it is
         * registered with the corresponding socket file descriptor to the
         * kernel to respond to events on the socket.
         * 
         */
        Socket( int fd, uint32_t userEventId );

        /* Destructor */
        virtual ~Socket() = 0;

        /**
         * @brief This method is overriden by a subclass to handle
         * and event on the underlying socket. The method will be called
         * whenever the kernel notifies the user of an event on this socket in
         * the relevant Socket Manager event loop.
         * 
         * @param ev The event on the socket fd that the kqueue alerted 
         * the user of.
         * @param socketManager Used to perform any actions in response to the
         * event ev.
         */
        virtual void handleSocketEvent( struct kevent& ev,
                                        SocketManager& socketManager ) = 0;
        
        void sendRPC( Test::TestMessage msg);
    
    protected:
        /**
         * @brief The socket file descriptor.
         */
        const int fd;

        /**
         * @brief Unique user event id used by Raft to signal to kernel that
         * a user triggered an event on the socket.
         * 
         */
        const uint32_t userEventId;

        /**
         * @brief Wrapper that stores the bytes read from socket buffer and
         * the number of bytes read from socket buffer.
         * 
         */
        class ReadBytes {
            public:
                ReadBytes(size_t numBytes);

                ~ReadBytes();

                /**
                 * @brief Number of bytes read in from socket buffer.
                 * 
                 */
                size_t numBytes;
                /**
                 * @brief Bytes Read in from socket buffer.
                 */
                char *bufferedBytes;
        };

        ReadBytes readBytes;

        /**
         * @brief Queue of RPCs to send to peer. This queue is checked when
         * Raft triggers a user event on the socket.
         */
        std::queue<Test::TestMessage> sendRPCQueue;


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
        ListenSocket( int fd, uint32_t userEventId, uint64_t firstRaftClientId );

        /* Destructor */
        ~ListenSocket();

        /**
         * @brief Method called whenever the kernel notifies us of an attempt to
         * connect to the Raft Server. If valid it will produce a ServerSocket
         * and add to the list of sockets that the kernel monitors. Otherwise 
         * fails silently.
         * 
         * @param ev The kernel event returned that indicates an attempt to
         * connect to the RaftServer.
         * @param socketManager Used to register the newly created server socket
         * to the kernel for monitoring.
         */
        void handleSocketEvent( struct kevent& ev,
                                SocketManager& socketManager );
    
    private:
        /**
         * @brief The Id to be assigned to the next RaftClient that connects
         * to this RaftServer.
         */
        uint64_t nextRaftClientId;
}; // class ListenSocket

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
        ServerSocket( int fd , uint32_t userEventId );

        /* Destructor */
        ~ServerSocket();

        /**
         * @brief Method called when the kernel notifies us that the socket 
         * client has sent data. If the data is a valid Raft RPC Request it 
         * replies with the corresponding Raft RPC response. Otherwise fails 
         * silently. 
         * 
         * @param ev The kernel event returned by kevent which indicates there is
         * data to be read from the socket's read buffer.
         * @param socketManager Currently unused, will be for responding to
         * the request.
         */
        void handleSocketEvent( struct kevent& ev,
                                SocketManager& socketManager );
}; // class ServerSocket

} // namespace Raft

#endif /* RAFT_SOCKET_H */