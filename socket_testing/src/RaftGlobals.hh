#ifndef RAFT_GLOBALS_H
#define RAFT_GLOBALS_H

#include <string>
#include <memory>
#include <libconfig.h++>
#include <unordered_map>
#include <netinet/in.h>
#include "Common/ServerConfig.hh"
#include "SocketManager.hh"
#include "Socket.hh"

namespace Raft {

    class ClientSocketManager;
    class ServerSocketManager;
    class Socket;

    class Globals {
        public:
            /**
             * @brief Construct a new Globals that stores the Global Raft State
            * 
             * @param configPath The path of the configuration file. 
             */
            Globals( std::string configPath );

            /* Destructor */
            ~Globals();

            /**
             * @brief Initialize a Globals with parameters from a configuration
             * file.
             */
            void init();

            /**
             * @brief Start the globals process
             */
            void start();

            /**
             * @brief All configuration parameters to be used by a RaftServer
             */
            Common::ServerConfig config;

            /**
             * @brief Service that handles any actions on sockets where the
             * RaftServer is the client i.e. it initiated the connection.
             */
            std::shared_ptr<ClientSocketManager> clientSocketManager;

            /**
             * @brief Service that handles any actions on sockets where the
             * RaftServer the server i.e. it did not initiate the connection.
             */
            std::shared_ptr<ServerSocketManager> serverSocketManager;



            bool addkQueueSocket(Socket* socket);

            bool removekQueueSocket(Socket* socket);

            int kq;

        
        private:

            /**
             * @brief Map of ID to address, I think this is better
             */
            std::unordered_map<int, sockaddr_in> clusterMap;

    }; // class Globals
} // namespace Raft

#endif /* RAFT_GLOBALS_H */