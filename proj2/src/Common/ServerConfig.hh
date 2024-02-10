#ifndef COMMON_SERVERCONFIG_H
#define COMMON_SERVERCONFIG_H

#include <string>
#include <netinet/in.h>

namespace Common {
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
             * @brief Address for RaftServer listen for incomming connection 
             * requests in form ip:port
             */
            std::string listenAddr;

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
             * @brief Maps Raft Server Id to an fully initialized socket address 
             * for all other RaftServers in this cluster. Socket address can be 
             * passed into a socket constructor without any additional 
             * configuration. These addresses are used for sending RaftServer 
             * Requests to other Raft Servers
             */
            std::unordered_map<uint64_t, std::string> clusterMap;


            /**
             * @brief Path of the file used for persistent storage.
             */
            std::string persistentStoragePath;

    }; // class ServerConfig
} // namespace Common

#endif /* COMMON_SERVERCONFIG_H */