#ifndef RAFT_GLOBALS_H
#define RAFT_GLOBALS_H

#include <string>
#include <memory>
#include <libconfig.h++>
#include <unordered_map>
#include <netinet/in.h>
#include <iostream>
#include "Common/ServerConfig.hh"
#include "RaftServer/Consensus.hh"
#include "RaftServer/LogStateMachine.hh"
// #include "RaftServer/SocketManager.hh"
// #include "RaftServer/Socket.hh"

#define FIRST_USER_EVENT_ID INT_MAX + 1

namespace Raft {

    class Consensus;
    class LogStateMachine;

    class Globals {
        public:
            /**
             * @brief Construct a new Globals that stores the Global Raft State.
            * 
             * @param configPath The path of the configuration file. 
             */
            Globals( std::string configPath );

            /* Destructor */
            ~Globals();

            /**
             * @brief Start the globals process
             */
            void start();

            /**
             * @brief Configuration data needed for a RaftServer
             */
            Common::ServerConfig config;

            /**
             * @brief Handles any actions on sockets where the RaftServer is 
             * the client i.e. it initiated the connection.
             */
            // std::shared_ptr<ClientSocketManager> clientSocketManager;

            /**
             * @brief Handles any actions on sockets where the RaftServer is 
             * the server i.e. it did not initiate the connection.
             */
            // std::shared_ptr<ServerSocketManager> serverSocketManager;

            /**
             * @brief Raft Consensus Unit: Figure 2 from Paper.
             */
            std::shared_ptr<Consensus> consensus;

            /**
             * @brief Log State Machine.
             */
            std::shared_ptr<LogStateMachine> logStateMachine;

            /**
             * @brief Generates user event id to be used for user triggered
             * events a socket manager kqueue. Starts as FIRST_USER_EVENT_ID
             * and value returned will monotomically increase. When this is
             * called nextUserEventId is incremented.
             * 
             * @return Unique User Event Id 
             */
            uint32_t genUserEventId();

        
        private:

            /**
             * Persistent Threads
            */
            // std::thread timerThread;
            // std::thread stateMachineUpdaterThread;
            std::vector<std::thread> persistentThreads;

            /**
             * @brief Map of ID to address, I think this is better
             */
            std::unordered_map<int, sockaddr_in> clusterMap;

            std::atomic<uint32_t> nextUserEventId;

    }; // class Globals
} // namespace Raft

#endif /* RAFT_GLOBALS_H */