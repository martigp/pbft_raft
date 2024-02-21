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

    // TODO: does anything need to get cleaned up?
    NetworkService::ConnectionState::~ConnectionState(){};

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
        // AF_INET
        if (inet_pton(AF_INET, ip.c_str(),
                                &(sockaddr->sin_addr)) <= 0) {
            throw Exception("Provided ip in " + addr + " could not be converted"
                            " to binary form needed for binding " +
                            std::strerror(errno));

        }
    }
    
    const std::string NetworkService::getAddrFromSocketFd(int socketFd) {
        
        for (const auto& [hostAddr, connectionState] : connectionStateMap) {
            if (connectionState->socketFd == socketFd) {
                return hostAddr;
            }
        }

        throw Exception("Socket " + std::to_string(socketFd) + 
                        "is not associated with a connected host");
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
            throw Exception(errorMsg + std::strerror(errno));
        }
    }

    // TODO: does anything need to get cleaned up
    NetworkService::~NetworkService() {
        if (close(pollFd) < 0)
        {
            std::cerr << "[Network] In desctructor error closing kqueue: " 
            << std::strerror(errno) << std::endl;
        }
    }

    void 
    NetworkService::sendMessage(const std::string sendToAddr,
                                const std::string msg,
                                bool createConnection) {
        std::thread sendMessageThread([&, sendToAddr, msg, createConnection] {
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
                    std::cerr << "Couldn't create socket for new connection with" <<
                    sendToAddr << " with error " << std::strerror(errno) << std::endl;
                    return;
                }

                // TODO: This may not be necessary for a client socket
                int opt = 1;
                if (setsockopt(sendSocketFd, SOL_SOCKET, SO_REUSEADDR, &opt,
                            sizeof(opt))) {
                    std::cerr << "Failed to setsockopt to reuse local addresses"
                                "for the new connection with: " + sendToAddr + 
                                "with error " << std::strerror(errno) << std::endl;
                    return;
                }

                struct sockaddr_in sendToSockAddr;
                try {
                    populateSockAddr(sendToAddr, &sendToSockAddr);
                }
                catch(Exception e) {
                    std::cerr << e.what() << ": closing socket to addr " 
                              << sendToAddr << std::endl;
                    close (sendSocketFd);
                    return;
                }

                if (connect(sendSocketFd,(struct sockaddr *)&(sendToSockAddr), 
                    sizeof(sendToSockAddr)) < 0) {
                    std::cout << "[Network] Failed to connect with host " 
                              << sendToAddr << " with error " <<
                              std::strerror(errno) <<  std::endl;
                    close(sendSocketFd);
                    return;
                }

                connectionState.reset(new ConnectionState(sendSocketFd));

                connectionStateMapLock.lock();
                connectionStateMap[sendToAddr] = connectionState;

                monitorSocketForEvents(sendSocketFd);

                std::cout << "[Network] connected to host " << sendToAddr << "Also"
                " and polling for responses" << std::endl;
            }
            else {
                connectionStateMapLock.unlock();
                return;
            }

            connectionStateMapLock.unlock();


            // Create the network message prefixing the message with a header
            // of how many bytes the payload is.
            uint64_t payloadLength = htonll(msg.size());
            char buf[HEADER_SIZE + msg.size()];
            memcpy(buf, &payloadLength, HEADER_SIZE);
            memcpy(buf + HEADER_SIZE, msg.c_str(), msg.size());

            connectionState->lock.lock();

            if (send(connectionState->socketFd, buf, sizeof(buf), 0) == -1) {
                connectionState->lock.unlock();
                std::cerr << "[Network] Error sending to host " << sendToAddr
                          << strerror(errno) << std::endl;
                removeConnection(sendToAddr);
                return;
            }

            std::cout << "[Network] Successfully sent message to " << sendToAddr
                      << " with payload size " << msg.size() <<std::endl;
            
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
            std::string errorMsg =
                "Failed to register socket " + std::to_string(socketFd) 
                + "to kqueue ";
            throw Exception(errorMsg + std::strerror(errno));
        }
    }

    void 
    NetworkService::removeConnection(const std::string& hostAddr) {

        std::cout << "[Network] Trying to remove host " << hostAddr << std::endl;
        connectionStateMapLock.lock();
        auto hostInfoPair = connectionStateMap.find(hostAddr);

        if (hostInfoPair != connectionStateMap.cend()) {
            // Remove from map but still have the shared pointer
            auto removedMapEntry = connectionStateMap.extract(hostInfoPair);
            connectionStateMapLock.unlock();

            auto connectionState = removedMapEntry.mapped();

            connectionState->lock.lock();
            if (connectionState->socketFd != INVALID_SOCKET_FD) {
                // Create event that removes socketFd from kqueue, event only
                // fully removed onced final file descriptor to socket is
                // closed.
                struct kevent ev;
                EV_SET(&ev, connectionState->socketFd, EVFILT_READ, EV_DELETE,
                    NULL, 0, NULL);
                if (kevent(pollFd, &ev, 1, NULL, 0, NULL) < 0) {
                    throw Exception("Failed to remove " + 
                        std::to_string(connectionState->socketFd) + "from kqueue");
                }
                close(connectionState->socketFd);
                connectionState->socketFd = INVALID_SOCKET_FD;
            }
            connectionState->lock.unlock();

            std::cout << "[Network] All state of host " << hostAddr <<
                " successfully removed." << std::endl;
        } else {
            // This is OK but still print message
            std::cerr << "[Network] Attempted to remove connection to " 
            << hostAddr << " but connection did not exists"
                      << std::endl;
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

                throw Exception(errorMsg + std::strerror(errno));
            }

            // Obtain the string version of the host IP address and port so
            // it can be added to the addrToHostInfo Map.
            char hostIp[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &(hostSockAddr.sin_addr.s_addr), hostIp,
                      INET_ADDRSTRLEN) == NULL) {
                close(hostSocketFd);
                std::cerr << "Failed to parse an incoming host connection"
                             "request's ip with inet_ntop: "
                          << std::strerror(errno) << std::endl;
                }

            uint16_t hostPort = ntohs(hostSockAddr.sin_port);

            const std::string hostAddr = 
                std::string(hostIp) + ':' + std::to_string(hostPort);
            
            const std::lock_guard<std::mutex> lg(connectionStateMapLock);
            if (connectionStateMap.find(hostAddr) != connectionStateMap.end()) {
                close(hostSocketFd);
                std::cerr << "Received duplicate request from host addr "
                          << hostAddr << ". Ignoring Conneciton request."
                          << std::endl;
                return;
            }
            connectionStateMap[hostAddr] = 
                std::make_shared<ConnectionState>(hostSocketFd);

            monitorSocketForEvents(hostSocketFd);
            
            std::cout << "[Network] Added successfully accepted connection"
            "request from " << hostAddr << "on socket " << hostSocketFd <<  std::endl;
            
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


        // Read in all availabile bytes
        while (totalBytesRead < socketBytesAvailable) {
            std::cout << "[Network] Total Bytes Read: " << totalBytesRead <<
            ". Bytes available to read: " << socketBytesAvailable << " Payload "
            "bytes needed " << connectionState->payloadBytesNeeded << 
            " Current Message bytes " << connectionState->msg.size() << std::endl;
            // Read the header of a network message first to determine how many
            // bytes to read for the payload.
            if (connectionState->msg.size() < HEADER_SIZE) {
                // Only read in header bytes worth
                size_t headerBytesToRead = HEADER_SIZE - totalBytesRead;
                char buf [HEADER_SIZE];

                ssize_t headerBytesRead = 
                    recv(receiveSocketFd, buf, headerBytesToRead, MSG_DONTWAIT);
                
                if (headerBytesRead == -1) {
                    std::cerr << "[Network] Error reading header bytes: " << 
                    std::strerror(errno) << ". Header bytes Needed: " 
                    << headerBytesRead << " bytes. Bytes Available: " 
                    << socketBytesAvailable - totalBytesRead << std::endl;
                    break;
                }

                totalBytesRead += headerBytesRead;
                connectionState->msg.append(buf, headerBytesRead);                
            }
            // If payload bytes has this default value, we still need to read
            // in its value from the header. Only do this when a full header
            // has arrived
            if (connectionState->payloadBytesNeeded == UNKNOWN_NUM_BYTES ) {
                if (connectionState->msg.size() == HEADER_SIZE) {
                    std::cout << "[Network] Received full header " << connectionState->msg << std::endl;
                    // Convert bytes to uint64_t and convert to correct ordering
                    // for network message
                    
                    // Nasty conversion but std::stoull was throwing errors.
                    uint64_t networkOrderedPayloadLen;
                    memcpy(&networkOrderedPayloadLen, connectionState->msg.c_str(), HEADER_SIZE);

                    connectionState->payloadBytesNeeded =
                        ntohll(networkOrderedPayloadLen);
   
                } else {
                    continue;
                }
            }

            // Full Header Received, now read in the payload.
            size_t payloadBytesToRead = connectionState->payloadBytesNeeded;
            char buf [payloadBytesToRead];

            ssize_t payloadBytesRead = 
                    recv(receiveSocketFd, buf, payloadBytesToRead, MSG_DONTWAIT);
                
            if (payloadBytesRead == -1) {
                std::cerr << "[Network] Error reading payload bytes: "
                << std::strerror(errno) << std::endl;
                break;
            }

            totalBytesRead += payloadBytesRead;
            connectionState->msg.append(buf, payloadBytesRead);
            connectionState->payloadBytesNeeded -= payloadBytesRead;

            // Payload fully arrived, forward only the payload to the 
            // Network User
            if (connectionState->payloadBytesNeeded == 0) {
                size_t payloadSize = connectionState->msg.size() - HEADER_SIZE;
                const std::string payload = 
                    connectionState->msg.substr(HEADER_SIZE, payloadSize);

                std::cout << "[Network] Passing complete message from " 
                << hostAddr << " to the Raft server" << std::endl;
                userCallbackFunction(hostAddr, payload);

                connectionState->msg = "";
                connectionState->payloadBytesNeeded = 
                    UNKNOWN_NUM_BYTES;
            }
        }
        // Intetinoally broke from the loop because of an error - remove the
        // connection
        if (totalBytesRead != socketBytesAvailable) {
            removeConnection(hostAddr);
        }
    }

    int
    NetworkService::createListenSocket(const std::string& listenAddr) {
        printf("[Network Service] Establishing listening socket on address %s\n",
                listenAddr.c_str());
        
        std::string errorMsg;
        int listenSocketFd = socket(AF_INET, SOCK_STREAM, 0);


        // Creating socket file descriptor
        if (listenSocketFd < 0) {
            errorMsg = "Failed to create socket to listen on: ";
            throw Exception(errorMsg + std::strerror(errno));
        }

        // Allowing the re-use of local addresses by other sockets
        int opt = 1;
        if (setsockopt(listenSocketFd, SOL_SOCKET, SO_REUSEADDR, &opt,
                    sizeof(opt))) {
            errorMsg = "Failed to setsockopt to reuse local addresses"
                        "for the listen socket: ";
            throw Exception(
                errorMsg + std::strerror(errno));
        }

        struct sockaddr_in listenSockAddr;
        populateSockAddr(listenAddr, &listenSockAddr);
            
        if (bind(listenSocketFd, (struct sockaddr*)&listenSockAddr,
                sizeof(listenSockAddr))
            < 0) {
            errorMsg = "Failed to bind provided listen address " 
                        + listenAddr + " to the created listen socket: ";
            throw Exception(
                errorMsg + std::strerror(errno));
        }

        if (listen(listenSocketFd, MAX_CONNECTIONS) < 0) {
            errorMsg = "Failure to listen on the bound listen socket: ";
            throw Exception(errorMsg + std::strerror(errno));
        }
        try {
            monitorSocketForEvents(listenSocketFd);
        }
        catch(Exception err) {
            errorMsg = "Failure to register the listen socket to the"
                        "kqueue: %s", err.what();
            throw Exception(errorMsg);
        }
        
        return listenSocketFd;
    }

    void
    NetworkService::startListening(const std::string& listenAddr)
    {
        int listenSocketFd;
        if (listenAddr == "") {
            listenSocketFd = INVALID_SOCKET_FD;
        } else {
            listenSocketFd = createListenSocket(listenAddr);
        }
        
        std::string errorMsg;

        while (true) {
            struct kevent evList[MAX_EVENTS];

            /* Poll for any events oc*/
            int numEvents = kevent(pollFd, NULL, 0, evList, MAX_EVENTS, NULL);

            if (numEvents == -1) {
                errorMsg = "Error polling sockets: ";
                throw Exception(errorMsg + std::strerror(errno));
            }

            for (int i = 0; i < numEvents; i++) {

                struct kevent ev = evList[i];

                int socketFd = (int) ev.ident;
                bool isListenSocket = socketFd == listenSocketFd;
                
                if (ev.flags & EV_ERROR) {
                    errorMsg = 
                        "Kqueue error when polling the %s event id: %s",
                        isListenSocket ? "Listen Socket" : std::to_string(socketFd),
                                                    std::strerror(ev.data);
                        throw Exception(errorMsg);
                }

                if (ev.filter == EVFILT_READ) {
                    std::string hostAddr;

                    if (!isListenSocket) {
                        // Extract the address associated with connection when not
                        // the listen socket.
                        try {
                            const std::lock_guard<std::mutex> lg(connectionStateMapLock);
                            hostAddr = getAddrFromSocketFd(socketFd);
                            std::cout << "[Network] Polled event from host " 
                                      << hostAddr << " on socket" << socketFd << std::endl;
                        }
                        catch (Exception e) {
                            std::cerr << "Socket " << socketFd
                                    << "not listen socket and not in connection "
                                        "map." << std::endl;
                            continue;
                        }
                    }

                    if (ev.data == 0 || ev.flags & EV_EOF) {
                        // Remove socket from polling by kqueue
                        removeConnection(hostAddr);
                    }
                    else {
                        // Normal operation
                        handleReceiveEvent(socketFd, ev.data, isListenSocket);
                    }
                    continue;
                } else {
                    // Event we did not register - this should never happen
                    throw Exception("Kqueue event with unknown identifier.");
                }
            }
        }
    }
}