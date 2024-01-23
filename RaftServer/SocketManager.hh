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
            SocketManager( Globals& globals );

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
            bool registerSocket( Socket* socket );

            /**
             * @brief 
             * 
             * @param socket Socket object to access when an event happens on
             * the corresponding file descriptor.
             * @return true 
             * @return false 
             */
            bool removeSocket( Socket* socket );
        
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

    }; // class SocketManager

    class ClientSocketManager: public SocketManager {
        public:
            /**
             * @brief Construct a new SocketManager that stores the Global Raft State
             */
            ClientSocketManager( Raft::Globals& globals );

            /* Destructor */
            ~ClientSocketManager();

            /**
             * @brief Initialize a SocketManager Listen Socket with parameters 
             * from our configuration file. 
             */
            void init();

            /**
             * @brief Start the IncomingSocketManager process
             */
            void start();
    }; // class IncomingSocketManager

    class ServerSocketManager: public SocketManager {
        public:
            /**
             * @brief Construct a new OutgoingSocketManager that stores the Global Raft State
             */
            ServerSocketManager( Raft::Globals& globals );

            /* Destructor */
            ~ServerSocketManager();

            /**
             * @brief Initialize outgoing manager state 
             */
            void init();

            /**
             * @brief Start the OutgoingSocketManager process
             */
            void start();

    }; // class OutgoingSocketManager

} // namespace Raft

#endif /* RAFT_SOCKETMANAGER_H */