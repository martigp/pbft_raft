#ifndef COMMON_NETWORK_SERVICE_CONFIG_H
#define COMMON_NETWORK_SERVICE_CONFIG_H

#include <string>
#include <netinet/in.h>

namespace Common {
    class NetworkServiceConfig {
        public:
            /**
             * @brief Constructor
             * @param configPath path to file with all configuration information
             * needed to establish a Network Service that a user can use.
             */
            NetworkServiceConfig(std::string configPath);

            /* Destructor */
            ~NetworkServiceConfig();

            /**
             * @brief Address for RaftServer listen for incomming connection 
             * requests.
             */
            std::string listenAddr;

            /**
             * @brief Network User Port used for all network communication.
             * 
             */
            int networkPort;

            /**
             * @brief The unique server id used to identify this NetworkUser
             * in the Network they are connecting to.
             * 
             */
            uint64_t serverId;

            /**
             * @brief The number of servers INCLUDING this network 
             * 
             */
            uint64_t numClusterServers;

            /**
             * @brief Maps a networkPeerId
             */
            std::unordered_map<uint64_t, sockaddr_in> clusterMap;


            /**
             * @brief Path of the file to add log entries. Used by Consensus
             * module.
             */
            std::string logPath;

    }; // class NetworkServiceConfig
} // namespace Common

#endif /* COMMON_NETWORK_SERVICE_CONFIG_H */