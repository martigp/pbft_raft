#include <sys/event.h>

namespace Raft {

/**
 * @brief A Socket is registered to an event loop and its method
 * handleSocketEvent() is called whenever an event occurs on the
 * socket.
 */
class Socket {
    public:
        /**
         * @brief The file descriptor of the socket.
         * 
         */
        const int fd;

        explicit Socket(int fd);

        /**
         * @brief This method is overriden by a subclass to handle
         * and event on the underlying socket. The method will be called
         * whenever the kernel notifies us of an event on this socket in
         * the event loop.
         * 
         * @param ev The event that occured on the socket that was
         * returned by the kqueue
         */
        virtual void handleSocketEvent(struct kevent ev) = 0;
};

/**
 * @brief A socket that listens for incoming connection requests on a Raft
 * Server and produces connections for the client to send Raft RPC requests.
 * 
 */
class ListenSocket :  Socket {
    /**
     * Constructor.
     * \param fd
     *      The file descriptor of the socket that is listening for incoming
     *      connections.
     */
    ListenSocket(int fd);

    /**
     * @brief Method called whenever the kernel notifies us of an attempt to
     * connect to the Raft Server. If valid it will produce a ServerSocket that
     * is added to the kqueue. Otherwise fails silently.
     * 
     * @param ev The kernel event returned that indicates an attempt to connect
     * to our RaftServer.
     */
    void handleSocketEvent(struct kevent ev);
};

/**
 * @brief A socket that listens for incoming Raft RPC requests.
 * 
 */
class ServerSocket : Socket {
    /**
     * Constructor.
     * @brief Construct a new Server Socket object that listens for RequestRPCs
     * from the corresponding client.
     * 
     * @param fd The file descriptor of a socket connected to a client.
     */
    ServerSocket(int fd);

    /**
     * @brief Method called when the kernel notifies us that the socket client
     * has sent data. If the data is a valid Raft RPC Request it reply with the
     * corresponding Raft RPC response. Otherwise fails silently. 
     * 
     * @param ev The kernel event returned by kevent which indicates there is
     * data to be read from the socket's read buffer.
     */
    void handleSocketEvent(struct kevent ev);
};

}