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
             */
            std::string addr;

            /**
             * @brief Port for RaftServer
             * 
             */
            uint64_t port;

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
             * TODO: I know we won't be using pair so need to replace this
             */
            std::unordered_map<uint64_t, std::pair<std::string, uint64_t>> clusterMap;

    }; // class ServerConfig
} // namespace Raft

#endif /* RAFT_SERVERCONFIG_H */