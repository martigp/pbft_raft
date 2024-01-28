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
             * @param id The id of the associated connection. For RaftServers
             * this is the serverId from config. For RaftClients this is a
             * monotonically increasing Id.
             * @param socket Socket object to access when an event happens on
             * the corresponding file descriptor.
             * @return Whether the socket was successfullly registered for
             * monitoring.
             */
            bool monitorSocket( uint64_t peerId, Socket *socket );

            /**
             * @brief 
             * 
             * @param socket Socket object to access when an event happens on
             * the corresponding file descriptor.
             * @return Whether the socket was successfully removed from sockets
             * that are monitored by the kernel.
             */
            bool stopSocketMonitor( Socket* socket );

            /**
             * @brief Adds a socket to 
             * 
             */
            void sendRPC(uint64_t peerId, google::protobuf::Message& msg,
                         Raft::RPCType rpcType);

            /**
             * @brief Method overriden by subclass that begins listening for 
             * kernel notifications about events on registered sockets and
             * reacts accordingly.
             */
            virtual void start() = 0;

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

            /* Unimplemented */
            void start();
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
             * @brief Begin listening for kernel notifications about events
             * on registered server sockets and react accordingly.
             */
            void start();

    }; // class ServerSocketManager

} // namespace Raft

#endif /* RAFT_SOCKETMANAGER_H */