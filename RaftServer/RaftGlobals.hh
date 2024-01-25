#ifndef RAFT_GLOBALS_H
#define RAFT_GLOBALS_H

#include <string>
#include <memory>
#include <jthread>
#include "SocketManager.hh"
#include "Consensus.hh"
#include "LogStateMachine.hh"
#include "raftrpc.pb.h"
#include "ServerConfig.hh"

using namespace RaftCommon;

namespace Raft {
    class Globals {
        public:
            /**
             * @brief Construct a new Globals that stores the Global Raft State
             */
            Globals();

            /* Destructor */
            ~Globals();

            /**
             * @brief Initialize a Globals with parameters from a configuration
             * file.
             * 
             * @param configPath The path of the configuration file. 
             * TODO: decide on having config in constructor for everything or in the init of everything
             */
            void init(std::string configPath);

            /**
             * @brief Start the globals process
             */
            void start();

            /**
             * @brief Process an RPC from the ServerSocketManager
             * Must be a request.
             * 
             * @param data String read from the socket fd.
             * 
             * @param serverID Unique RaftServer or RaftClient ID assocaied with the socket that was read from.
             * 
             * @returns Serialized string to write back to caller
             */
            std::string processRPCReq(std::string data, int serverID);

            /**
             * @brief Process an RPC from the ClientSocketManager
             * Must be a response to a request.
             * 
             * @param data String read from the socket fd.
             * 
             * @param serverID Unique RaftServer or RaftClient ID assocaied with the socket that was read from.
             */
            void processRPCResp(std::string data, int serverID);

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
            ServerConfig config;

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
            std::map<NamedThread::ThreadType, NamedThread> threadMap;

    }; // class Globals

    class NamedThread {
        public:
            /**
             * @brief Thread object.
             */ 
            std::jthread thread;

            /**
             * Enum for: TimerThread, IncomingListeningThread, OutgoingReqThread(many)
             * , OutgoingListeningThread, (potentially) WaitingForAppliedThread
            */
            enum class ThreadType {
                TIMER,
                SERVERLISTENING,
                CLIENTLISTENING,
                CLIENTINITIATED
            };

            /**
             * @brief State of this server
            */
            ThreadType myType;
    }; // class NamedThread

} // namespace Raft

#endif /* RAFT_GLOBALS_H */