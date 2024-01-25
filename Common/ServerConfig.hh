/**
 * Kinda just sketching a rough idea of creating a config object that we can construct in the Raft Server Globals 
 * or in the Client constructor as well. Idk if necessary? Common namespace might be nice to have as well
*/

#ifndef COMMON_SERVERCONFIG_H
#define COMMON_SERVERCONFIG_H

#include <string>
#include <netinet/in.h>

namespace RaftCommon {
    class ServerConfig {
        public:
            /**
             * @brief Construct a new ServerConfig that stores the Raft ServerConfig State
             */
            ServerConfig(std::string configPath);

            /* Destructor */
            ~ServerConfig();

            /**
             * @brief My serverID
            */
            int myServerID;

            /**
             * @brief My server address
            */
            sockaddr_in myServerAddress;

            /**
             * @brief Addresses of the servers
             */
            std::vector<sockaddr_in> addresses;

            /**
             * @brief List of IDs
             */
            std::vector<int> serverIds;

            /**
             * @brief Map of ID to address, I think this is better
             */
            std::unordered_map<int, sockaddr_in> clusterMap;

        private:

    }; // class ServerConfig
} // namespace RaftCommon

#endif /* COMMON_SERVERCONFIG_H */