#include <arpa/inet.h>
#include <iostream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <cerrno>
#include <cstdio>
#include <unistd.h>
#include <thread>
#include "Common/NetworkService.hh"


namespace Common {

    NetworkService::ConnectionState::ConnectionState(int socketFd)
        : socketFd(socketFd),
          lock(),
          msg(""),
          payloadBytesNeeded(UNKNOWN_NUM_BYTES) {}


    void 
    NetworkService::populateSockAddr(const std::string &addr,
                                     struct sockaddr_in * sockaddr) {
        
        std::string ip;
        uint16_t port;

        std::istringstream iss(addr);
        // Use getline to split the string by ':'
        std::getline(iss, ip, ':');
        iss >> port;

        sockaddr->sin_family = AF_INET;
        sockaddr->sin_port = htons(port);

        // Copy the ip address to the sockaddr so we can bind the socket
        // to the provided address.
        if (inet_pton(AF_INET, ip.c_str(),
                                &(sockaddr->sin_addr)) <= 0) {
            std::string errorMsg = 
                "Failed to convert provided ip in address " + addr +
                " to binary form needed for binding: ";
            throw std::runtime_error(errorMsg + std::strerror(errno));
        }
    }
    
    const std::string NetworkService::getAddrFromSocketFd(int socketFd) {
        
        for (const auto& [hostAddr, connectionState] : connectionStateMap) {
            if (connectionState->socketFd == socketFd) {
                return hostAddr;
            }
        }

        std::string errorMsg("Socket " + std::to_string(socketFd) + 
                             "is not associated with a connected host");

        throw std::runtime_error(errorMsg);
    }

    NetworkService::NetworkService(NetworkUser& user)
        : connectionStateMap(),
          connectionStateMapLock()
    {
        userCallbackFunction = std::bind(&NetworkUser::handleNetworkMessage,
                                         &user,
                                         std::placeholders::_1,
                                         std::placeholders::_2);

        // Set up the kqueue that socket file descriptors can be registered to.
        pollFd = kqueue();
        if (pollFd == -1) {
            std::string errorMsg =
                "Failure to create the kqueue for the NetworkService: ";
            throw std::runtime_error(errorMsg + std::strerror(errno));
        }
    }

    void 
    NetworkService::sendMessage(const std::string& sendToAddr,
                                const std::string& msg,
                                bool createConnection,
                                const std::string& sendFromAddr) {

        std::thread sendMessageThread([&] {
            connectionStateMapLock.lock();                         

            // Should only be one entry in map for each address, if this
            // count is greater than 0, we are already connected to this address
            bool connectedToRecipient = connectionStateMap.count(sendToAddr) > 0;
            std::shared_ptr<ConnectionState> connectionState;

            if (connectedToRecipient) {
                connectionState = connectionStateMap[sendToAddr];
            }
            else if (createConnection) {
                connectionStateMapLock.unlock();
                
                int sendSocketFd = socket(AF_INET, SOCK_STREAM, 0);
                if (sendSocketFd  < 0) {
                    return;
                }

                struct sockaddr_in sendToSockAddr;
                try {
                    // A send address was specified so bind the socket to this
                    // address.
                    if (sendFromAddr != "") {
                        struct sockaddr_in sendFromSockAddr;
                        populateSockAddr(sendFromAddr, &sendFromSockAddr);

                        if (bind(sendSocketFd,
                                 (struct sockaddr *)&sendFromAddr,
                                 sizeof(sendFromSockAddr) < 0)) {
                            throw std::runtime_error(
                                "Failed to bind specified send address" + 
                                sendFromAddr + "to socket");
                        }

                    }
                    populateSockAddr(sendToAddr, &sendToSockAddr);
                }
                catch(std::exception e) {
                    close (sendSocketFd);
                    return;
                }

                if (connect(sendSocketFd,(struct sockaddr *)&(sendToSockAddr), 
                    sizeof(sendToSockAddr)) < 0) {
                    close(sendSocketFd);
                    return;
                }

                connectionState.reset(new ConnectionState(sendSocketFd));

                connectionStateMapLock.lock();
                connectionStateMap[sendToAddr] = connectionState;

                monitorSocketForEvents(sendSocketFd);
            }
            else {
                connectionStateMapLock.unlock();
                return;
            }

            connectionStateMapLock.unlock();


            // Create the network message prefixing the message with a header
            // of how many bytes the payload is.
            uint64_t payloadLength = htonll(msg.size());
            char buf[HEADER_SIZE + payloadLength];
            memcpy(buf, &payloadLength, HEADER_SIZE);
            memcpy(buf + HEADER_SIZE, msg.c_str(), payloadLength);


            connectionState->lock.lock();

            if (send(connectionState->socketFd, buf, sizeof(buf), 0) == -1) {
                connectionState->lock.unlock();
                removeHost(sendToAddr);
                return;
            }
            
            connectionState->lock.unlock();
        });

        sendMessageThread.detach();                            
    }

    void 
    NetworkService::monitorSocketForEvents(int socketFd) {
        // At this point the socket should already be in the 
        // addrToHostInfo mapping.
        struct kevent newEv;
        bzero(&newEv, sizeof(newEv));

        // Setting the event struct to let the kqueue know only to signal
        // when a new incoming connection requests for a listen socket OR/
        // new bytes received on a connected socket.
        EV_SET(&newEv, socketFd, EVFILT_READ, EV_ADD, 0, 0, NULL);

        // Failed to add throw exception.
        if (kevent(pollFd, &newEv, 1, NULL, 0, NULL) == -1) {
            throw std::runtime_error(std::strerror(errno));
        }
    }

    void 
    NetworkService::removeHost(const std::string& hostAddr) {

        connectionStateMapLock.lock();
        auto hostInfoPair = connectionStateMap.find(hostAddr);

        if (hostInfoPair != connectionStateMap.cend()) {
            // Remove from map
            hostInfoPair = connectionStateMap.erase(hostInfoPair);
            connectionStateMapLock.unlock();

            auto connectionState = hostInfoPair->second;

            // Closing the socket removes it from the kqueue as well
            // Someone may have gotten the entry from the map already, make
            // sure it is unusable by setting socketFd to invalid.
            connectionState->lock.lock();
            close(connectionState->socketFd);
            connectionState->socketFd = INVALID_SOCKET_FD;
            connectionState->lock.unlock();
        } else {
            connectionStateMapLock.unlock();
        }
    }

    void
    NetworkService::handleReceiveEvent(int receiveSocketFd,
                                       uint64_t eventInformation,
                                       bool isListenSocket)
    {   
        // Handle event on a listen socket by attempting to add 
        if (isListenSocket) {
            struct sockaddr_in hostSockAddr;
            socklen_t hostAddrLen;
            std::string errorMsg;

            // Accept connection and read host information into hostSockAddr
            int hostSocketFd =
                accept(receiveSocketFd, (struct sockaddr *)&hostSockAddr,
                    &hostAddrLen);
            
            if (hostSocketFd < 0) {
                errorMsg = "Failed to accept an incoming connection: ";

                throw std::runtime_error(errorMsg + std::strerror(errno));
            }

            // Obtain the string version of the host IP address and port so
            // it can be added to the addrToHostInfo Map.
            char hostIp[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &(hostSockAddr.sin_addr), hostIp,
                      INET_ADDRSTRLEN) ) {
                close(hostSocketFd);
                errorMsg = "Failed to parse an incoming host connection"
                           "request's ip with inet_ntop: %s", 
                           std::strerror(errno);
                throw std::runtime_error(errorMsg);
            }

            uint16_t hostPort = ntohs(hostSockAddr.sin_port);

            const std::string hostAddr = hostIp + std::to_string(hostPort);
            
            const std::lock_guard<std::mutex> lg(connectionStateMapLock);
            connectionStateMap[hostAddr] = 
                std::make_shared<ConnectionState>(hostSocketFd);
            
            return;
        } // ListenSocket Event

        // Connected Host Socket Read Event, read bytes from socket
        size_t socketBytesAvailable = eventInformation;
        size_t totalBytesRead = 0;
        
        connectionStateMapLock.lock();
        std::string hostAddr = getAddrFromSocketFd(receiveSocketFd);
        std::shared_ptr<ConnectionState> connectionState = 
            connectionStateMap[hostAddr];
        connectionStateMapLock.unlock();


        // This is to read the prefix of a network message which is the
        // number of bytes in the payload
        while (totalBytesRead < socketBytesAvailable) {
            // Read attempt to read header size bytes
            if (connectionState->msg.size() < HEADER_SIZE) {
                // Only read in header bytes worth
                size_t headerBytesToRead = HEADER_SIZE - totalBytesRead;
                char buf [headerBytesToRead];

                ssize_t headerBytesRead = 
                    recv(receiveSocketFd, buf, headerBytesToRead, MSG_DONTWAIT);
                
                if (headerBytesRead == -1) {
                    break;
                }

                totalBytesRead += headerBytesRead;
                connectionState->msg += buf;                
            }
            // Read in complete header but have not parsed the header yet since
            // the payloadBytesNeeded is still its default value.
            if (connectionState->msg.size() == HEADER_SIZE &&
                connectionState->payloadBytesNeeded == UNKNOWN_NUM_BYTES) {

                // Convert bytes to uint64_t and convert to correct ordering
                // for network message
                uint64_t networkOrderedPayloadLen = std::stoull(connectionState->msg);
                connectionState->payloadBytesNeeded = ntohll(networkOrderedPayloadLen);
            }

            size_t payloadBytesToRead = connectionState->payloadBytesNeeded;
            char buf [payloadBytesToRead];

            ssize_t payloadBytesRead = 
                    recv(receiveSocketFd, buf, payloadBytesToRead, MSG_DONTWAIT);
                
            if (payloadBytesRead == -1) {
                break;
            }

            totalBytesRead += payloadBytesRead;
            connectionState->msg += buf;
            connectionState->payloadBytesNeeded -= payloadBytesRead;

            if (connectionState->payloadBytesNeeded == 0) {
                size_t payloadSize = connectionState->msg.size() - HEADER_SIZE;
                const std::string payload = 
                    connectionState->msg.substr(HEADER_SIZE, payloadSize);

                userCallbackFunction(hostAddr, payload);

                connectionState->msg = "";
                connectionState->payloadBytesNeeded = 
                    UNKNOWN_NUM_BYTES;
            }
        }

        removeHost(hostAddr);
    }

    int
    NetworkService::createListenSocket(const std::string& listenAddr) {
        std::string errorMsg;

        int listenSocketFd = socket(AF_INET, SOCK_STREAM, 0);

        // Creating socket file descriptor
        if (listenSocketFd < 0) {
            errorMsg = "Failed to create socket to listen on: ";
            throw std::runtime_error(errorMsg + std::strerror(errno));
        }

        // Allowing the re-use of local addresses by other sockets
        int opt = 1;
        if (setsockopt(listenSocketFd, SOL_SOCKET, SO_REUSEADDR, &opt,
                    sizeof(opt))) {
            errorMsg = "Failed to setsockopt to reuse local addresses"
                        "for the listen socket: ";
            throw std::runtime_error(
                errorMsg + std::strerror(errno));
        }

        struct sockaddr_in listenSockAddr;
        populateSockAddr(listenAddr, &listenSockAddr);
            
        if (bind(listenSocketFd, (struct sockaddr*)&listenSockAddr,
                sizeof(listenSockAddr))
            < 0) {
            errorMsg = "Failed to bind provided listen address " 
                        + listenAddr + " to the created listen socket: ";
            throw std::runtime_error(
                errorMsg + std::strerror(errno));
        }

        if (listen(listenSocketFd, MAX_CONNECTIONS) < 0) {
            errorMsg = "Failure to listen on the bound listen socket: ";
            throw std::runtime_error(errorMsg + std::strerror(errno));
        }
        try {
            monitorSocketForEvents(listenSocketFd);
        }
        catch(std::runtime_error err) {
            errorMsg = "Failure to register the listen socket to the"
                        "kqueue: %s", err.what();
            throw std::runtime_error(errorMsg);
        }
        
        return listenSocketFd;
    }

    void
    NetworkService::startListening(const std::string& listenAddr)
    {
        int listenSocketFd = createListenSocket(listenAddr);
        std::string errorMsg;

        while (true) {
            struct kevent evList[MAX_EVENTS];

            /* Poll for any events oc*/
            int numEvents = kevent(pollFd, NULL, 0, evList, MAX_EVENTS, NULL);

            if (numEvents == -1) {
                errorMsg = "Error polling sockets: ";
                throw std::runtime_error(errorMsg + std::strerror(errno));
            }

            for (int i = 0; i < numEvents; i++) {

                struct kevent ev = evList[i];
                int socketFd = (int) ev.ident;

                std::string hostAddr;

                bool isListenSocket = socketFd == listenSocketFd;

                if (!isListenSocket) {
                    const std::lock_guard<std::mutex> lg(connectionStateMapLock);
                    hostAddr = getAddrFromSocketFd(socketFd);
                }

                if (ev.fflags & EV_ERROR) {
                    // Error on listen socket is Fatal. Shut down server.
                    if (isListenSocket) {
                        errorMsg = 
                            "Kqueue error when polling the listen socket: %s",
                            std::strerror(ev.data);
                        throw std::runtime_error(errorMsg);
                    }
                }
                // EV_EOF signifies a host closed the connection. This cannot
                // happen on a listen socket.
                else if (ev.fflags & EV_EOF) {
                    std::cerr << "Host " << hostAddr << "closed connection."
                              << std::endl;
                }
                // An event we registered occured on the socket occured
                else if (ev.filter == EVFILT_READ) {
                    handleReceiveEvent(socketFd, ev.data, isListenSocket);
                    continue;
                }
                // If wasn't a normal read event we want to remove the host.
                removeHost(hostAddr);
            }
        }
    }
}