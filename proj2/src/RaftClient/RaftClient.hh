#define CONFIG_PATH "./config_client.cfg"

namespace Raft {
    class ClientConfig;

    class RaftClient {
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

        private:
            /**
             * @brief Configuration object constructed for a RaftClient
            */
            ClientConfig config;
    }
}