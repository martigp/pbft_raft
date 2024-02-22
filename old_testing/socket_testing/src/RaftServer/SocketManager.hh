#ifndef RAFT_SOCKETMANAGER_H
#define RAFT_SOCKETMANAGER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <sys/event.h>
#include "RaftServer/Socket.hh"
#include "RaftServer/RaftGlobals.hh"
#include "Protobuf/RaftRPC.pb.h"
#include "Common/RPC.hh"
#include "Common/ThreadPool.hh"

#define MAX_CONNECTIONS 10
#define MAX_EVENTS 1
#define LISTEN_SOCKET_ID 0

namespace Raft {

    // Forward declare the Globals.
    class Globals;
    class Socket;

    /**
     * @brief Responsible for registering sockets to the kernel for monitoring
     * of events and reacting to kernel notifications of events on those
     * sockets.
     */
    class SocketManager {
        public:
            /**
             * @brief Construct a new SocketManager that stores the "global 
             * socket kqueue" state
             */
            SocketManager( Globals& globals );

            /* Destructor */
            virtual ~SocketManager();

            /**
             * @brief Register a socket to be monitored by the kernel. Any 
             * future calls to kevent will alert user if there were any events
             * on the socket.
             * 
             * @param socket Socket object to access when an event happens on
             * the corresponding file descriptor.
             * @return Whether the socket was successfullly registered for
             * monitoring.
             */
            void monitorSocket(Socket *socket );

            /**
             * @brief 
             * 
             * @param socket Socket object to access when an event happens on
             * the corresponding file descriptor.
             * @return Whether the socket was successfully removed from sockets
             * that are monitored by the kernel.
             */
            void stopSocketMonitor( Socket* socket );

            /**
             * @brief Adds a socket to 
             * 
             */
            void sendRPC(uint64_t peerId, google::protobuf::Message& msg,
                         Raft::RPCType rpcType);

            /**
             * @brief Method overriden by subclass. Called when a peerId is
             * found but the corresponding socket pointer is NULL.
             * @param peerId Peer Id of the socket with missing socket pointer.
             */
            virtual void handleNoSocketEntry(uint64_t peerId) = 0;

            /**
             * @brief Begins listening for kernel notifications about events on
             *  registered sockets and corresponding socket reacts accordingly.
             */
            void start();

            /**
             * @brief The file descriptor of the kqueue that alerts a RaftServer
             * of events on any open sockets the kqueue monitors.
             */
            int kq;
        
            /**
             * @brief Reference to server globals. Used to handle RPC requests.
             */
            [[maybe_unused]] Raft::Globals& globals;
            
            /**
             * @brief Keeps track sockets so can be deleted and removed from
             * kq during destruction. Alternatively could just be FDs because
             * we just need to close them, but then removal is hard?
             * 
             */
            std::unordered_map<uint64_t, Socket *> sockets;

    }; // class SocketManager

    /**
     * @brief Responsible for Client Sockets. In the case of a RaftServer,
     * Client Sockets correspond to connections the RaftServer initiated with
     * other RaftServers in order to send RPC Requests to them.
     * 
     */
    class ClientSocketManager: public SocketManager {
        public:
            /**
             * @brief Constructor
             * 
             * @param globals Used to access global state
             */
            ClientSocketManager( Raft::Globals& globals );

            /* Destructor */
            ~ClientSocketManager();

            /**
             * @brief For a client socket this indicates the connection has 
             * shut down. Creates a new socket 
             */
            void handleNoSocketEntry(uint64_t peerId);

        private:
            /**
             * @brief Threadpool responsible for spawning threads that are
             * responsible for sending RPCs over the socket and maintaining the
             * connection.
             */
            ThreadPool threadpool;
    }; // class ClientSocketManager


    class ServerSocketManager: public SocketManager {
        public:
            /**
             * @brief Constructor
             * 
             * @param globals Used to acces RaftServer global state
             */
            ServerSocketManager( Raft::Globals& globals );

            /**
             * @brief Destructor 
             * 
             * TODO: should remove all events and delete associated SocketPtr
             */
            ~ServerSocketManager();

            /**
             * @brief Causes crash, this should not happen.
             * 
             */
            void handleNoSocketEntry(uint64_t peerId);

    }; // class ServerSocketManager

} // namespace Raft

#endif /* RAFT_SOCKETMANAGER_H */