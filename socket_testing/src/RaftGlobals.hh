#ifndef RAFT_GLOBALS_H
#define RAFT_GLOBALS_H

#include <string>
#include <memory>
#include <libconfig.h++>
#include <unordered_map>
#include <netinet/in.h>
#include "socket.hh"

namespace Raft {
    class Globals {
        public:
            /**
             * @brief Construct a new Globals that stores the Global Raft State
             */
            Globals();

            /* Destructor */
            ~Globals();

            /**
             * @brief Initialize a Globals with parameters from a configuration
             * file.
             * 
             * @param configPath The path of the configuration file. 
             */
            void init(std::string configPath);

            /**
             * @brief Register a socket to be monitored by the kernel. Any 
             * future calls to kevent will return is there were any events on 
             * the socket.
             * 
            * @param socket Socket object to access when an event happens on
             * the corresponding file descriptor.
             * @return Whether the file descriptor was registered for
             * monitoring.
             */
            bool addkQueueSocket(Socket* socket);

            /**
             * @brief Stop monitoring a socket.
             * 
             * @param socket Socket object to access when an event happens on
             * the corresponding file descriptor.
             * @return true 
             * @return false 
             */
            bool removekQueueSocket(Socket* socket);
        
            /**
             * @brief The file descriptor of the kqueue that alerts a RaftServer
             * of events on any open sockets the kqueue monitors.
             */
            int kq;

            /**
             * @brief Port used by Raft
             */
            int raftPort;

            /**
             * @brief Address to listen for incoming connections
             * 
             */
            std::string listenAddr;
        
        private:

            /**
             * @brief Map of ID to address, I think this is better
             */
            std::unordered_map<int, sockaddr_in> clusterMap;

    }; // class Globals
} // namespace Raft

#endif /* RAFT_GLOBALS_H */