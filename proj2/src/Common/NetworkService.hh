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
/* Socket Fd value for HostConnectionState if the socket is closed */
#define INVALID_SOCKET_FD -1
/* The default value of payloadBytesNeeded member of HostConnectionState.
   Indicates a full header has not been read in yet. */
#define UNKNOWN_NUM_BYTES -1
/* The size of a header. The header is just an unsigned long indicating the
   number of bytes in the following payload. */
#define HEADER_SIZE sizeof(uint64_t)

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
             * @brief API used to send a message a known peer in the network.
             * This will attempt to send the message asynchronously and fail
             * silently if the message could not be sent.
             * 
             * @param sendAddr Address the message should be sent to in the
             * form ip:port
             * @param msg The message to be sent to the peer
             * 
             */
            void sendMessage(const std::string& sendAddr,const std::string& msg);

            /**
             * @brief Method called to start the network service listening
             * on the provided listen address. Throws an error on failure
             * of setting up the listen socket or an error on the listen
             * socket.
             *
             */
            void startListening(const std::string& listenAddr);

        private:

            /**
             * @brief Registers the socket to the kqueue to monitor for read
             * events. Throws error if unable to register it with string
             * version of the error returned by the kqueue.
             * 
             * @param socketFd The file descriptor of the socket.
             * 
             */
            void monitorSocketForEvents(int socketFd);
            
            /**
             * @brief Remove any trace of a connected host.
             * 
             * @param hostAddr The host address in form ip:port
             */
            void removeHost(const std::string& hostAddr);

            /**
             * @brief Handles the event that there is data available to read
             * from the socket. 
             * 
             * If the isListenSocket is set will attempt to establish new
             * connection. Throws exception if an issue and it is a listen
             * socket. 
             * 
             * If a connectedSocket (not listen socket) will pass on any
             * complete messages to the user. Fails silently.
             * 
             * @param receiveSocketFd The file descriptor of the socket on which
             * the event occured.
             * @param eventInformation The information about the event returned
             * by the kqueue.
             * @param isListenSocket True if the event occured on the listen
             * socket. Used to interpret the eventInformation
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
             * addr. Throws exception if failure to do so.
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
             * has the hostStateMap lock.
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
             * 
             */
            class HostConnectionState {
                public:
                    HostConnectionState(int socketFd);

                    ~HostConnectionState();
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
             * with the connection. If the host is not present there is no 
             * existing connection to that host.
             */
            std::unordered_map<const std::string,
                              std::shared_ptr<HostConnectionState>> 
                              hostStateMap;

            /**
             * @brief Synchronization for hostStateMap operations. This lock
             * can be released once you get access to a values in the map
             * (a shared pointer). But all read and write to the map itself
             * require holding this lock.
             */
            std::mutex hostStateMapLock;

    }; // class NetworkService

} // namespace Common

#endif /* COMMON_NETWORSERVICE_H */