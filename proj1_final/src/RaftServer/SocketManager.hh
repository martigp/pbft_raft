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
     * sockets. A RaftServer has both a ClientSocketManager and a
     * ServerSocketManager that override this class.
     */
    class SocketManager {
        public:
            /**
             * @brief Constructor. Reference global needed to call other Raft
             * Modules in response to socket events.
             */
            SocketManager( Globals& globals );

            /* Destructor */
            virtual ~SocketManager();

            /**
             * @brief Register a socket to the socket manager.
             * 
             * @param socket Socket object that returned by the kernel whenever
             * an event happens on the socket's file descriptor. Used to do
             * response to event.
             */
            void monitorSocket(Socket *socket );

            /**
             * @brief Removes all records of the socket from the socket manager
             * and destroys the object. Any accesses following this are to
             * unallocated memory.
             * 
             * @param socket socket to be removed from socket manager records.
             */
            void stopSocketMonitor( Socket* socket );

            /**
             * @brief API used to send an RPC. Upon return, message has been
             * added to the queue of the relevant socket and the socket is
             * signalled it has an RPC queued to send. 
             * 
             * @param peerID a unique identifier of peer that a socket is
             * connected to. For RaftClients this Id is found out upon receipt of
             * an RPC request.
             * @param msg The rpc to send, represented as a base class.
             * @param rpcType The type of RPC (AppendEntries, RequestVote,
             * StateMachineCmd)
             * 
             */
            void sendRPC(uint64_t peerId, google::protobuf::Message& msg,
                         Raft::RPCType rpcType);

            /**
             * @brief Method overriden by subclass. Called when a peerId matches
             * a socket manager's records but it doesn't have access to the
             * connection with that peer.
             * @param peerId SocketManager's identifier a connection with a peer.
             */
            virtual void handleNoSocketEntry(uint64_t peerId) = 0;

            /**
             * @brief Starts listen loop on the thread passed in.
             */
            void startListening(std::thread &listeningThread);

            /**
             * @brief Listens for kernel notifications about events on
             *  registered sockets and corresponding socket reacts accordingly.
             */
            void listenLoop();

            /**
             * @brief The file descriptor of the kqueue that alerts a RaftServer
             * of events on any open sockets the kqueue monitors.
             */
            int kq;
        
            /**
             * @brief Reference to server globals. Used to handle RPC requests.
             */
            Raft::Globals& globals;
            
            /**
             * @brief Used for bookkeeping. Maps a uniquer identifier of a peer
             * to a socket wrapper.
             * 
             */
            std::unordered_map<uint64_t, Socket *> sockets;

    }; // class SocketManager

    /**
     * @brief Responsible for Client Sockets. In the case of a RaftServer,
     * Client Sockets correspond to connections the RaftServer initiated with
     * other RaftServers in order to send RPC Requests to them and listen
     * for responses.
     * 
     */
    class ClientSocketManager: public SocketManager {
        public:
            /**
             * @brief Constructor. Initiates persistent attempts to connect
             * to all other RaftServers in the cluster.
             */
            ClientSocketManager( Raft::Globals& globals );

            /* Destructor */
            ~ClientSocketManager();

            /**
             * @brief Called whenever we want to send something to another 
             * RaftServer but there is no connected socket. Initiates attempts
             * to connect to Raft Server identified by.
             * @param peerId The serverId of a RaftServer
             */
            void handleNoSocketEntry(uint64_t peerId);

        private:
            /**
             * @brief Threadpool used allocate threads to each Client connection
             * to a Raft Server so that operations with Raft Servers can happen
             * in parallel.
             */
            ThreadPool threadpool;
    }; // class ClientSocketManager


    /**
     * @brief Manages all connections initiated by the peer of the
     * connection. This can be both RaftClient and RaftServer. Note all RPCs
     * coming on these connections will be RPC requests and all RPCs sent will
     * be RPC responses.
     */
    class ServerSocketManager: public SocketManager {
        public:
            /**
             * @brief Constructor
             *
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