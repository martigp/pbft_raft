#ifndef RAFT_GLOBALS_H
#define RAFT_GLOBALS_H

#include <string>
#include <memory>
#include "SocketManager.hh"
#include "Consensus.hh"
#include "LogStateMachine.hh"

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
             * @brief Incoming Manager.
             */
            std::shared_ptr<Raft::IncomingSocketManager> incomingSockets;

            /**
             * @brief Outgoing Manager.
             */
            std::shared_ptr<Raft::OutgoingSocketManager> outgoingSockets;

            /**
             * @brief Raft Consensus Unit: Figure 2 from Paper.
             */
            std::shared_ptr<Raft::Consensus> raftConsensus;

            /**
             * @brief Log State Machine.
             */
            std::shared_ptr<Raft::LogStateMachine> stateMachine;

    }; // class Globals
} // namespace Raft

#endif /* RAFT_GLOBALS_H */