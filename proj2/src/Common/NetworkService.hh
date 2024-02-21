#ifndef COMMON_NETWORKSERVICE_H
#define COMMON_NETWORKSERVICE_H

#include <string>
#include <memory>
#include <unordered_map>
#include <sys/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <mutex>
#include "Common/NetworkUser.hh"

/* The backlog of connections a server can sustain. Somewhat arbitrary. */
#define MAX_CONNECTIONS 16
/* The maximum number of the kqueue can return. Somewhat arbitrary */
#define MAX_EVENTS 1
/* Socket Fd value for ConnectionState if the socket has been closed */
#define INVALID_SOCKET_FD -1
/* The default value of payloadBytesNeeded member of ConnectionState.
   Indicates a full header has not been read in yet. */
#define UNKNOWN_NUM_BYTES -1
/* The size of a header. The header is just an unsigned long indicating the
   number of bytes in the following payload. */
#define HEADER_SIZE sizeof(uint64_t)
/* Flag used to specify to the Network that a connection should be created
   to the specified recipient if one does not exist already. */
#define CREATE_CONNECTION true

namespace Common {

    // Forward declare the Application.
    class NetworkUser;

    /**
     * @brief Service responsible for conducting network communication needed by
     * the Network User.
     */

    class NetworkService {
        public:
        /**
         * @brief Exception defined for the network class.
         * 
         */ 
            class Exception : public std::runtime_error {
                public:
                 Exception(const std::string err) : std::runtime_error(err){};
            };
         /**
          * @brief Construct a new Network Service. Runtime error thrown
          * there is a error setting up the Network Service that would not
          * allow it to provide the service.
          *
          * @param networkUser The user of the network service.
          *
          */
         NetworkService(NetworkUser& networkUser);

         /* Destructor */
         ~NetworkService();

         /**
          * @brief Send a message to the provided network address.
          * This will attempt to send the message asynchronously and fail
          * silently if the message could not be sent.
          *
          * @param sendToAddr Address the message should be sent to in the
          * form ip:port
          * @param msg The message to be sent to the peer
          * @param createConnection TRUE if user wants to create connection
          * with the sendToAddr if one does not exist. By default FALSE.
          *
          */
         void sendMessage(const std::string sendToAddr, const std::string msg,
                          bool createConnection = false);

         /**
          * @brief Method called to start the network service listening
          * for incoming connection requests. Throws an error on failure
          * of setting up the listen socket or an error on the listen
          * socket.
          * 
          * @param listenAddr The address to listen for incoming requests on.
          */
         void startListening(const std::string& listenAddr);

        private:

            /**
             * @brief Registers a socket to the Network Service. Throws 
             * exception if unable to register. Upon return, socket is registerd
             * and any well-formed data received on this socket is handed to the
             * associated Network User.
             * 
             * @param socketFd The file descriptor of the socket.
             * 
             */
            void monitorSocketForEvents(int socketFd);
            
            /**
             * @brief Remove any trace of a connected host from NetworkService.
             * 
             * @param hostAddr The host address in form ip:port
             */
            void removeConnection(const std::string& hostAddr);

            /**
             * @brief Handles the event that there is data available to read
             * from the socket. 
             * 
             * If the isListenSocket is set, will attempt to establish new
             * connection. Throws exception if an issue and it is a listen
             * socket. 
             * 
             * If an already connectedSocket (not listen socket) will complete 
             * messages to the user. Fails silently.
             * 
             * @param receiveSocketFd The file descriptor of the socket on which
             * the event occured.
             * @param eventInformation The information about the event returned
             * by the kqueue.
             * @param isListenSocket True if the event occured on the listen
             * socket. False if a socket associated with an existing connection.
             */
            void handleReceiveEvent(int receiveSocketFd,
                                    uint64_t eventInformation,
                                    bool isListenSocket);

            /**
             * @brief Create a listen socket that listens on provided
             * listenAddr. Throws an exception on failure.
             * 
             * @param listenAddr Address in form ip:port
             * @return int The file descriptor of the listening socket.
             */
            int createListenSocket(const std::string& listenAddr);


            /**
             * @brief Populates the sockaddr with the ip and port provided in
             * addr. Throws exception if failure.
             * 
             * @param addr Address in form ip:port
             * @param sockaddr The sockaddr to be populated
             * @return ip The returned ip address 
             * @return port The returned port
             */
            void populateSockAddr(const std::string &addr,
                                  struct sockaddr_in *sockaddr);

            /**
             * @brief Returns the address of the host connected to by the
             * socket. Since these operations are generally followed by use
             * of map THIS IS NOT THREAD SAFE i.e. it assumes that the caller 
             * has the connectionStateMap lock.
             * 
             * @param socketFd The file descriptor of the socket connected to
             * a host.
             * @return const std::string The host address in form ip:port
             */
            const std::string getAddrFromSocketFd(int socketFd);

            /**
             * @brief The file descriptor which the kernel uses to notify the
             * Network Service of activity on any of the file descriptors of
             * sockets that are registered to it to listen for events on.
             */
            int pollFd;

            /**
             * @brief State associated with a host's connection for reading
             * and writing data to the host.
             */
            class ConnectionState {
                public:
                    ConnectionState(int socketFd);

                    ~ConnectionState();
                    /**
                     * @brief The file descriptor of the socket that is
                     * connected to the host.
                     */
                    int socketFd;
                    /** 
                     * @brief Lock to ensure synchronous access and modification
                     * of the host's connection state.
                     */
                    std::mutex lock;

                    /**
                     * @brief Stores the bytes of a message received on host
                     * socket. Will at most store 1 complete message at a time.
                     * 
                     */
                    std::string msg;

                    /**
                     * @brief The number of bytes from payload needed,
                     * UNKNOWN_NUM_BYTES (-1) if we have not read in a complete
                     * header and so don't know the size of the payload.
                     * 
                     */
                    ssize_t payloadBytesNeeded;
            };
        
            /**
             * @brief Callback function to pass on any messages received by
             * the Network Service to the Network User.
             */
            std::function<void(const std::string&,
                               const std::string)> userCallbackFunction;
            
            /**
             * @brief Maps a host address in form ip:port to state assocaited
             * with a connection with that address.
             */
            std::unordered_map<std::string,
                              std::shared_ptr<ConnectionState>> 
                              connectionStateMap;

            /**
             * @brief Synchronization for connectionStateMap operations. This lock
             * can be released once you get access to a values in the map
             * (a shared pointer). But all read and write to the map itself
             * require holding this lock.
             */
            std::mutex connectionStateMapLock;

    }; // class NetworkService

} // namespace Common

#endif /* COMMON_NETWORSERVICE_H */