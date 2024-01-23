#ifndef RAFT_GLOBALS_H
#define RAFT_GLOBALS_H

#include <string>
#include <memory>
#include <libconfig.h++>
#include <unordered_map>
#include <netinet/in.h>
#include "Common/ServerConfig.hh"
#include "socket.hh"

namespace Raft {
    class Globals {
        public:
            /**
             * @brief Construct a new Globals that stores the Global Raft State
            * 
             * @param configPath The path of the configuration file. 
             */
            Globals( std::string configPath );

            /* Destructor */
            ~Globals();

            /**
             * @brief Initialize a Globals with parameters from a configuration
             * file.
             */
            void init();

            /**
             * @brief Start the globals process
             */
            void start();

            /**
             * @brief All configuration parameters to be used by a RaftServer
             */

            Common::ServerConfig config;

            /**
             * @brief Port used by Raft
             */
            int raftPort;

            /**
             * @brief Address to listen for incoming connections
             * 
             */
            std::string listenAddr;

            bool addkQueueSocket(Socket* socket);

            bool removekQueueSocket(Socket* socket);

            int kq;

        
        private:

            /**
             * @brief Map of ID to address, I think this is better
             */
            std::unordered_map<int, sockaddr_in> clusterMap;

    }; // class Globals
} // namespace Raft

#endif /* RAFT_GLOBALS_H */