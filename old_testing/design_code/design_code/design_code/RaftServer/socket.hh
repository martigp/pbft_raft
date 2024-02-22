#ifndef RAFT_SOCKET_H
#define RAFT_SOCKET_H

#include <sys/event.h>
#include "SocketManager.hh"

namespace Raft {

/**
 * @brief A Socket is registered to an event loop and its method
 * handleSocketEvent() is called whenever an event occurs on the
 * socket.
 */

class SocketManager;

class Socket {
    public:
        /**
         * @brief Construct a new Socket object
         */
        Socket(int fd, SocketManager* socketManager);

        /* Destructor */
        virtual ~Socket() = 0;

        /**
         * @brief This method is overriden by a subclass to handle
         * and event on the underlying socket. The method will be called
         * whenever the kernel notifies us of an event on this socket in
         * the event loop.
         * 
         * @param ev The event that occured on the socket that was
         * returned by the kqueue
         */
        virtual void handleSocketEvent(struct kevent& ev) = 0;

        /**
         * @brief The file descriptor of the socket.
         */
        const int fd;

        /**
         * @brief A pointer to the raft SocketManager used to perform actions
         * in response to events on a socket.
         */
        SocketManager* socketManager;
}; // class Socket

/**
 * @brief A socket that listens for incoming connection requests on a Raft
 * Server and produces connections for the client to send Raft RPC requests.
 * 
 */
class ListenSocket : public Socket {
    public:
        /**
         * @brief Construct a new Listen Socket object that listens for incoming
         * connection requests.
         * 
         * @param fd The file descriptor of the socket.
         * @param socketManager The SocketManager State.
         */
        ListenSocket(int fd, SocketManager* socketManager);

        /* Destructor */
        ~ListenSocket();

        /**
         * @brief Method called whenever the kernel notifies us of an attempt to
         * connect to the Raft Server. If valid it will produce a ServerSocket that
         * is added to the kqueue. Otherwise fails silently.
         * 
         * @param ev The kernel event returned that indicates an attempt to connect
         * to our RaftServer.
         */
        void handleSocketEvent(struct kevent& ev);
}; // class ListenSocket

/**
 * @brief A socket that listens for incoming Raft RPC requests.
 * 
 */
class ServerSocket : public Socket {
    public:
        /**
         * @brief Construct a new Server Socket object that listens for messages
         * from the corresponding client from an existing socket file
         * descriptor.
         * 
         * @param fd The file descriptor of a socket connected to a client.
         * @param socketManager The SocketManager State.
         */
        ServerSocket(int fd, SocketManager* socketManager);

        /* Destructor */
        ~ServerSocket();

        /**
         * @brief Method called when the kernel notifies us that the socket client
         * has sent data. If the data is a valid Raft RPC Request it reply with the
         * corresponding Raft RPC response. Otherwise fails silently. 
         * 
         * @param ev The kernel event returned by kevent which indicates there is
         * data to be read from the socket's read buffer.
         */
        void handleSocketEvent(struct kevent& ev);
}; // class ServerSocket

} // namespace Raft

#endif /* RAFT_SOCKET_H */