#ifndef COMMON_SERVERCONFIG_H
#define COMMON_SERVERCONFIG_H

#include <string>
#include <netinet/in.h>

namespace Common {
    class ServerConfig {
        public:
            /**
             * @brief Construct a new ServerConfig that stores the Raft ServerConfig State
             */
            ServerConfig(std::string configPath);

            /* Destructor */
            ~ServerConfig();

            /**
             * @brief My server ID.
             * 
             */
            int serverID;


            /**
             * @brief Address to listen for incomming connection requests.
             * 
             */
            std::string listenAddr;

            /**
             * @brief Raft Port
             * 
             */
            int raftPort;

            /**
             * @brief Map of ID to address, I think this is better
             */
            std::unordered_map<uint64_t, sockaddr_in> clusterMap;

    }; // class ServerConfig
} // namespace Common

#endif /* COMMON_SERVERCONFIG_H */