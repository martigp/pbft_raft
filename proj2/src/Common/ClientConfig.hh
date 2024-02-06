#ifndef COMMON_CLIENTCONFIG_H
#define COMMON_CLIENTCONFIG_H

#include <string>
#include <netinet/in.h>

namespace Common {
    class ClientConfig {
        public:
            /**
             * @brief Constructor
             * @param configPath path to file with all Raft Server cluster
             * configuation information.
             */
            ClientConfig(std::string configPath);

            /* Destructor */
            ~ClientConfig();

            /**
             * @brief Address for RaftClient
             */
            std::string addr;

            /**
             * @brief Port for RaftClient
             * 
             */
            int port;

            /**
             * @brief The number of servers INCLUDING the current server.
             * 
             */
            uint64_t numClusterServers;

            /**
             * @brief Maps Raft Server Id to an fully initialized socket address 
             * for all other RaftServers in this cluster. Socket address can be 
             * passed into a socket constructor without any additional 
             * configuration. These addresses are used for sending RaftServer 
             * Requests to other Raft Servers
             */
            std::unordered_map<uint64_t, sockaddr_in> clusterMap;

    }; // class ClientConfig
} // namespace Common

#endif /* COMMON_CLIENTCONFIG_H */