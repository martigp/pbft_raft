/**
 * Kinda just sketching a rough idea of creating a config object that we can construct in the Raft Server Globals 
 * or in the Client constructor as well. Idk if necessary? Common namespace might be nice to have as well
*/

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
            ServerConfig(std::string config_path);

            /* Destructor */
            ~ServerConfig();

        private:
            /**
             * @brief List of addresses
             */
            std::vector<sockaddr_in> addresses;

            /**
             * @brief List of IDs
             */
            std::vector<sockaddr_in> serverIds;

    }; // class ServerConfig
} // namespace Common

#endif /* COMMON_SERVERCONFIG_H */