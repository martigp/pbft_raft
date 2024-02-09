#ifndef RAFT_SERVERCONFIG_H
#define RAFT_SERVERCONFIG_H

#include <string>
#include <netinet/in.h>

namespace Raft {
    class ServerConfig {
        public:
            /**
             * @brief Constructor
             * @param configPath path to file with all Raft Server configuation
             * information.
             */
            ServerConfig(std::string configPath);

            /* Destructor */
            ~ServerConfig();

            /**
             * @brief Address for RaftServer
             * Formatted: "xx.xx.xx.xx:port"
             */
            std::string ipAddr;

            /**
             * @brief Raft Server Id of this Raft Server.
             * 
             */
            uint64_t serverId;

            /**
             * @brief The number of servers INCLUDING the current server.
             * 
             */
            uint64_t numClusterServers;

            /**
             * @brief Maps Raft Server Id to address and port pair
             */
            std::unordered_map<uint64_t, std::string> clusterMap;

    }; // class ServerConfig
} // namespace Raft

#endif /* RAFT_SERVERCONFIG_H */