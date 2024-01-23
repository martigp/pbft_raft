#ifndef RAFT_INCOMINGSOCKETCONNECTIONS_H
#define RAFT_INCOMINGSOCKETCONNECTIONS_H

#include <string>
#include <memory>
#include "socket.hh"

namespace Raft {
    class IncomingSocketConnections {
        public:
            /**
             * @brief Construct a new IncomingSocketConnections that stores the Global Raft State
             */
            IncomingSocketConnections();

            /* Destructor */
            ~IncomingSocketConnections();

            /**
             * @brief Initialize a IncomingSocketConnections with parameters from a configuration
             * file.
             * 
             * @param config The ServerConfig object passed in by the globals. 
             */
            void init(ServerConfig config);

            /**
             * @brief Start the IncomingSocketConnections process
             */
            void start();

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
             * @brief 
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
        
        private:

            /**
             * @brief The ServerConfig object after configPath is parsed.
             * Used mainly for the "self" field
             */
            Common::ServerConfig config;

    }; // class IncomingSocketConnections
} // namespace Raft

#endif /* RAFT_INCOMINGSOCKETCONNECTIONS_H */