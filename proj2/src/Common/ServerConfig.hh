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
             * @brief Address for RaftServer to send RPC requests from and 
             * receive corresponding RPC responses on. Addr in form ip:port
             */
            std::string clientAddr;

            /**
             * @brief Address for RaftServer to listen for RPC requests on and 
             * send corresponding RPC responses from. Addr in form ip:port
             */
            std::string serverAddr;

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
             * @brief Maps Raft Server Id to two addresses in the form of
             * ip:port. The first address is the address to send RPC Requests to
             * and the second address is the address to send RPC Responses to
             * for that server.
             */
            std::unordered_map<uint64_t, std::string> clusterMap;


            /**
             * @brief Path of the file used for persistent storage.
             */
            std::string persistentStoragePath;

    }; // class ServerConfig
} // namespace Common

#endif /* COMMON_SERVERCONFIG_H */