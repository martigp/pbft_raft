#include <string>
#include "RaftClient/ClientConfig.hh"
#include "Protobuf/RaftRPC.pb.h"
#include "Common/NetworkService.hh"

#define CONFIG_PATH "./config_client.cfg"

namespace Raft {
    class ClientConfig;

    class RaftClient : public Common::NetworkUser {
        public:
            /**
             * @brief Construct a new RaftClient that is able to connect 
             * to a Raft Cluster 
             * Will exit with error if configuration file path specified
             * above is not found.
             */
            RaftClient();

            /* Destructor */
            ~RaftClient();

            std::string connectAndSendToServer(std::string *in);

            /**
             * @brief Overriden function that is called by the Network Service
             * that the RaftServer is a user of when it has received a message.
             * 
             * @param sendAddr Host address from which a message was received
             * Formatted: "xx.xx.xx.xx:port"
             * 
             * @param networkMsg Network message received
            */
            void handleNetworkMessage(const std::string& sendAddr,
                                      const std::string& networkMsg);

        private:
            /**
             * @brief Configuration object constructed for a RaftClient
            */
            Raft::ClientConfig config;

            /**
             * @brief The service used by the Raft Server to send and receive
             * messages on the network.
             */
            Common::NetworkService network;
    };
}