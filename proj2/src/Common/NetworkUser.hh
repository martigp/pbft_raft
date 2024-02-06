#ifndef COMMON_NETWORK_USER_H
#define COMMON_NETWORK_USER_H

#include <string>
#include <memory>
#include <libconfig.h++>
#include <unordered_map>
#include <netinet/in.h>
#include "Common/NetworkServiceConfig.hh"

namespace Common {

    class NetworkUser {
        public:
            /**
             * @brief Construct a new NetworkUser.
            * 
             * @param networkConfigPath The path of the configuration file with
             * all information required to use a networkService
             */
            NetworkUser( std::string networkConfigPath );

            /* Destructor */
            ~NetworkUser();

            /**
             * @brief This method is overriden by the derrived class to handle
             * any messages that are received by the Network Service.
             * 
             * @param peerId The unique identifier of the message received by
             * the network.
             * @param msg A message received by the Network Service.
             */
            virtual void handleNetworkMessage(uint64_t peerId,
                                              std::string& msg) = 0;

            /**
             * @brief Configuration data needed for the NetworkService the user
             * will plug into.
             */
            Common::NetworkServiceConfig config;

    }; // class NetworkUser
} // namespace Common

#endif /* COMMON_NETWORK_USER_H */