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
             */
            std::string addr;

            /**
             * @brief Port for RaftClient
             * 
             */
            uint64_t port;

            /**
             * @brief The number of servers INCLUDING the current server.
             * 
             */
            uint64_t numClusterServers;

            /**
             * @brief Maps Raft Server Id to address and port pair
             * TODO: I know we won't be using pair so need to replace this
             */
            std::unordered_map<uint64_t, std::pair<std::string, uint64_t>> clusterMap;

    }; // class ClientConfig
} // namespace Raft

#endif /* RAFT_CLIENTCONFIG_H */