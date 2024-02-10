#ifndef RAFT_CLIENTCONFIG_H
#define RAFT_CLIENTCONFIG_H

#include <string>
#include <netinet/in.h>

namespace Raft {
    class ClientConfig {
        public:
            /**
             * @brief Constructor for a Raft Client configuraiton
             * 
             * @param configPath path to file with all Raft Server cluster
             * configuation information.
             * 
             * @returns Object with easily accessible address and port
             * information for the cluster.
             * Will exit if file is not found or is unable to be read.
             */
            ClientConfig(std::string configPath);

            /* Destructor */
            ~ClientConfig();

            /**
             * @brief Address for RaftClient
             * Formatted: "xx.xx.xx.xx:port"
             */
            std::string ipAddr;

            /**
             * @brief The number of servers in the Raft Cluster.
             * 
             */
            uint64_t numClusterServers;

            /**
             * @brief Maps Raft Server Id to address and port pair
             */
            std::unordered_map<uint64_t, std::string> clusterMap;

    }; // class ClientConfig
} // namespace Raft

#endif /* RAFT_CLIENTCONFIG_H */