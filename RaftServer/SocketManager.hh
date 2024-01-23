#ifndef RAFT_SOCKETMANAGER_H
#define RAFT_SOCKETMANAGER_H

#include <string>
#include <memory>
#include "socket.hh"
#include "RaftGlobals.hh"

namespace Raft {
    // use name SocketManager instead of Globals as you did in your socket_testing
    // we will have two of these 
    class SocketManager {
        public:
            /**
             * @brief Construct a new SocketManager that stores the "global socket kqueue" state
             */
            SocketManager();

            /* Destructor */
            ~SocketManager();

            /**
             * @brief Initialize a SocketManager Listen Socket with parameters 
             * from our configuration file. 
             */
            void init();

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
             * @brief Reference to server globals
            */
            Raft::Globals& globals;

            /**
             * @brief The ServerConfig object after configPath is parsed.
             * Used mainly for the "self" field
             */
            Common::ServerConfig config;

    }; // class SocketManager

    class IncomingSocketManager: public SocketManager {
        public:
            /**
             * @brief Construct a new SocketManager that stores the Global Raft State
             */
            IncomingSocketManager( Raft::Globals& globals, Common::ServerConfig config );

            /* Destructor */
            ~IncomingSocketManager();

            /**
             * @brief Initialize a SocketManager Listen Socket with parameters 
             * from our configuration file. 
             */
            void init();

            /**
             * @brief Start the IncomingSocketManager process
             */
            void start();
        
        private:
            /**
             * @brief Reference to server globals
            */
            Raft::Globals& globals;

            /**
             * @brief The ServerConfig object after configPath is parsed.
             * Used mainly for the "self" field
             */
            Common::ServerConfig config;

    }; // class IncomingSocketManager

    class OutgoingSocketManager: public SocketManager {
        public:
            /**
             * @brief Construct a new OutgoingSocketManager that stores the Global Raft State
             */
            OutgoingSocketManager( Raft::Globals& globals, Common::ServerConfig config );

            /* Destructor */
            ~OutgoingSocketManager();

            /**
             * @brief Initialize outgoing manager state 
             */
            void init();

            /**
             * @brief Start the OutgoingSocketManager process
             */
            void start();
        
        private:
            /**
             * @brief Reference to server globals
            */
            Raft::Globals& globals;

            /**
             * @brief The ServerConfig object after configPath is parsed.
             * Used mainly for the "self" field
             */
            Common::ServerConfig config;

    }; // class OutgoingSocketManager

} // namespace Raft

#endif /* RAFT_SOCKETMANAGER_H */