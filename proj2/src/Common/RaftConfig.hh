#ifndef COMMON_RAFTCONFIG_H
#define COMMON_RAFTCONFIG_H

#include <string>

namespace Common {

    enum RaftHostType {
        CLIENT,
        SERVER
    };

    class RaftConfig {
        public:

            /**
             * @brief Constructor
             * @param configPath path to file with all Raft configuation
             * information.
             */
            RaftConfig(std::string configPath, RaftHostType type);

            /* Destructor */
            ~RaftConfig();

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

    }; // class RaftConfig
} // namespace Common

#endif /* COMMON_RAFTCONFIG_H */