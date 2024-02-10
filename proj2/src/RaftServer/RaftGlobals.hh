#ifndef RAFT_GLOBALS_H
#define RAFT_GLOBALS_H

#include <string>
#include <memory>
#include <libconfig.h++>
#include <unordered_map>
#include <netinet/in.h>
#include "Common/ServerConfig.hh"
#include "Common/NetworkUser.hh"
#include "Common/NetworkService.hh"
#include "RaftServer/Socket.hh"
#include "RaftServer/Consensus.hh"
#include "RaftServer/LogStateMachine.hh"

#define FIRST_USER_EVENT_ID INT_MAX + 1

namespace Raft {

    class ClientSocketManager;
    class ServerSocketManager;
    class Socket;
    class Consensus;
    class LogStateMachine;

    class Globals : public Common::NetworkUser {
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

            void handleNetworkMessage(const std::string& receiveAddr,
                                              const std::string msg);

            /**
             * @brief Configuration data needed for a RaftServer
             */
            Common::ServerConfig config;

            /**
             * @brief Service to send and receive messages from network. 
             */
            Common::NetworkService networkService;

            /**
             * @brief Raft Consensus Unit: Figure 2 from Paper.
             */
            Consensus consensus;

            /**
             * @brief Log State Machine.
             */
            LogStateMachine logStateMachine;

        
        private:
            /**
             * @brief Main Persistent Threads(Timer, StateMachine, CSM kqueue, SSM kqueue)
             * Between these four and a threadpool for outbound client connections,
             * all of Raft is maintained
            */
            std::vector<std::thread> mainThreads;

            /**
             * @brief Map of RaftServer ID to address
             */
            std::unordered_map<int, std::string> clusterMap;

            /**
             * @brief Next Unique User Event Id
             */
            std::atomic<uint32_t> nextUserEventId;

    }; // class Globals
} // namespace Raft

#endif /* RAFT_GLOBALS_H */