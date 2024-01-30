#ifndef RAFT_GLOBALS_H
#define RAFT_GLOBALS_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include "SocketManager.hh"
#include "Consensus.hh"
#include "LogStateMachine.hh"
#include "raftrpc.pb.h"
#include "ServerConfig.hh"

using namespace Common;
using namespace RaftCommon;

namespace Raft {
    class Globals {
        public:
            /**
             * @brief Construct a new Globals that stores the Global Raft State
             * Initialize a Globals with parameters from a configuration file.
             * 
             * @param configPath The path of the configuration file.
             * NOTE: I need the config because the RaftServer threads need to be initialized
             * as there is no copy constructor for threads
             */
            Globals(std::string configPath);

            /* Destructor */
            ~Globals();

            /**
             * @brief Start the globals process
             */
            void start();

            /**
             * @brief Process an RPC from the ServerSocketManager
             * Must be a request.
             * 
             * @param data RaftRPC protobuf read from socket fd
             * 
             * @param serverID Unique RaftServer or RaftClient ID assocaied with the socket that was read from.
             * 
             * @returns RaftRPC protobuf to write back to socket fd
             */
            RaftRPC processRPCReq(RaftRPC req, int serverID);

            /**
             * @brief Process an RPC from the ClientSocketManager
             * Must be a response to a request.
             * 
             * @param data RaftRPC protobuf read from socket fd
             * 
             * @param serverID Unique RaftServer or RaftClient ID assocaied with the socket that was read from.
             */
            void processRPCResp(RaftRPC resp, int serverID);

            /**
             * @brief Broadcast an RPC to all other servers
             * 
             * @param data Serialized RPC string
             */
            void broadcastRPC(std::string data);
        
        private:

            /**
             * @brief The path of the configuration file to be read for Raft
             * initialization.
             */
            std::string configPath;

            /**
             * @brief The ServerConfig object after configPath is parsed.
             */
            Common::ServerConfig config;

            /**
             * @brief Server Socket Manager.
             */
            std::shared_ptr<Raft::ServerSocketManager> serverSockets;

            /**
             * @brief Client Socket Manager.
             */
            std::shared_ptr<Raft::ClientSocketManager> clientSockets;

            /**
             * @brief Raft Consensus Unit: Figure 2 from Paper.
             */
            std::shared_ptr<Raft::Consensus> raftConsensus;

            /**
             * @brief Log State Machine.
             */
            std::shared_ptr<Raft::LogStateMachine> stateMachine;

            /**
             * @brief Track persistent threads spun up on start().
            */
            NamedThread timerThread;
            NamedThread serverListeningThread;
            NamedThread clientListeningThread;
            NamedThread stateMachineUpdaterThread;

            /**
             * @brief One thread for each outbound RaftServer client connection 
             * that may need to be stopped if a new request must be sent
            */
            std::vector<NamedThread> raftServerThreads;

    }; // class Globals

    class NamedThread {
        public:
            /**
             * @brief Thread object.
             */ 
            std::thread thread;

            /**
             * Enum for: TimerThread, IncomingListeningThread, OutgoingReqThread(many)
             * , OutgoingListeningThread, (potentially) WaitingForAppliedThread
            */
            enum class ThreadType {
                TIMER,
                SERVERLISTENING,
                CLIENTLISTENING,
                STATEMACHINEUPDATER,
                CLIENTINITIATED
            };

            /**
             * @brief State of this server
            */
            ThreadType myType;

            /**
             * @brief Request stop.
             */ 
            std::atomic<bool> stop_requested = false;

            /**
             * @brief Request stop.
             */ 
            std::atomic<bool> closed = false;
    }; // class NamedThread

} // namespace Raft

#endif /* RAFT_GLOBALS_H */