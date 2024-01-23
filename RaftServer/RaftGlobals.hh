#ifndef RAFT_GLOBALS_H
#define RAFT_GLOBALS_H

#include <string>
#include <memory>
#include "socket.hh"

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
             */
            void init(std::string configPath);

            /**
             * @brief Start the globals process
             */
            void start();

            /**
             * @brief Register a socket to be monitored by the kernel. Any 
             * future calls to kevent will return is there were any events on 
             * the socket.
             * 
            * @param socket Socket object to access when an event happens on
             * the corresponding file descriptor.
             * @return Whether the file descriptor was registered for
             * monitoring.
             */
            bool addkQueueSocket(Socket* socket);

            /**
             * @brief 
             * 
             * @param socket Socket object to access when an event happens on
             * the corresponding file descriptor.
             * @return true 
             * @return false 
             */
            bool removekQueueSocket(Socket* socket);
        
            /**
             * @brief The file descriptor of the kqueue that alerts a RaftServer
             * of events on any open sockets the kqueue monitors.
             */
            int kq;
        
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