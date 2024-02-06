#ifndef COMMON_NETWORKSERVICE_H
#define COMMON_NETWORKSERVICE_H

#include <string>
#include <memory>
#include <unordered_map>
#include <sys/event.h>
#include <mutex>
#include "Common/NetworkServiceConfig.hh"
#include "Common/NetworkUser.hh"

#define MAX_CONNECTIONS 10
#define MAX_EVENTS 1
#define LISTEN_SOCKET_ID 0

namespace Common {

    // Forward declare the Application.
    class NetworkUser;

    /**
     * @brief Service responsible for conducting network communication needed by
     * the Network User. The user will provide a cluster network users that
     * it should be able to send and receive messages to by default. The Network
     * Service will also listen for any incoming connections from both network
     * users in the cluster and outside of it. If these connections are
     * successfully established, its network user may send and receive messages
     * to it as well.
     */
    class NetworkService {
        public:
            /**
             * @brief Constructor.
             * 
             * @param user Reference to the user that is plugging into the
             * network. This will be used to obtain configuation information
             * for the NetworkService to start running and pass on any messages
             * received on the Network Service.
             * 
             */
            NetworkService( NetworkUser& user );

            /* Destructor */
            virtual ~NetworkService();

            /**
             * @brief API used to send a message a known peer in the network.
             * This will attempt to send the message asynchronously and fail
             * silently if the message could not be sent.
             * 
             * @param peerID a unique identifier of peer that the user wants to
             *  send a message to.
             * @param msg The message to be sent to the peer
             * 
             */
            void sendMessage(uint64_t peerId, std::string& msg);

            /**
             * @brief Method called to start up the network service.
             * 
             * @param listeningThread The thread that listens for incoming
             * connection requests and messages to the Network Service.
             */
            void startListening(std::thread &listeningThread);

        private:

            /**
             * @brief Add a socket to the set of sockets that the NetworkService
             * is polling for incoming messages. If the file descriptor is
             * already being polled will fail silently.
             * 
             * @param socketFd The file descriptor of the socket that we want
             * to monitor.
             */
            void monitorSocket(int socketFd);

            /**
             * TODO: figure out what this does with respect to closing a socket
             * if we close the socket, this will automatically be done for us.
             * @brief Removes all records of the socket from the socket manager
             * and destroys the object. Any accesses following this are to
             * unallocated memory.
             * 
             * @param socketFd The file descriptor of the socket that we want
             * to stop monitoring.
             */
            void stopMonitorSocket( int socketfd );

            /**
             * TODO: Determine if this method is still needed!
             * @param peerId The unique identifier for the peer that does not
             * have an associated socket.
             */
            void handleNoSocketEntry(uint64_t peerId);

            /**
             * @brief The function run by the thread provided as an argument
             * to startListening. This will listen for any incoming connection
             * requests and messages on connections established by the Network
             * Service.
             */
            void listenLoop();

            /**
             * @brief The file descriptor which the kernel uses to notify the
             * Network Service of activity on any of the file descriptors of
             * sockets that it listening or connected to. All new connections
             * are registered to this pollFd.
             */
            int pollFd;

            /**
             * @brief The information about each Network peer required to
             * communicate with a that peer using the Network Service.
             * 
             */
            struct peerInformation {
                /**
                 * @brief The file descriptor of the socket associated with the
                 * peer. Set to -1 if the peer is in the cluster the Network
                 * Service is plugged into but the Network Service is not
                 * connected to it.
                 */
                int socketFd;
                /**
                 * @brief Lock to ensure that actions on a socket file
                 * descriptor are synchronized.
                 * 
                 */
                std::mutex sendToPeerLock;
            };
        
            /**
             * @brief Reference to user of this the Network Service. Used to
             * access NetworkService configuration information and to pass on
             * messages received by the NetworkService.
             */
            Common::NetworkUser& user;
            
            /**
             * @brief Maps a uniquer identifier of a peer to all the information
             * required to communicate with that peer. Initially populated with
             * information about all members of the cluster that the Network
             * Service is joining.
             * 
             */
            std::unordered_map<uint64_t, peerInformation> sockets;

    }; // class NetworkService

} // namespace Common

#endif /* COMMON_NETWORSERVICE_H */