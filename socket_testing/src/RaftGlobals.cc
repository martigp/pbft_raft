#include <string>
#include <sys/event.h>
#include <libconfig.h++>
#include <iostream>
#include <cstdlib>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "RaftGlobals.hh"
#include "socket.hh"

namespace Raft {
    
    Globals::Globals()
    {        
        kq = kqueue();
        if (kq == -1) {
            perror("Failed to create kqueue");
            exit(EXIT_FAILURE);
        }
    }

    Globals::~Globals()
    {
    }

    void Globals::init(std::string configPath) {

        libconfig::Config cfg;

        // Read the file. If there is an error, report it and exit.
        try
        {
            cfg.readFile(configPath);
        }
        catch(const libconfig::FileIOException &fioex)
        {
            std::cerr << "I/O error while reading file." << std::endl;
            exit(EXIT_FAILURE);
        }
        catch(const libconfig::ParseException &pex)
        {
            std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
                    << " - " << pex.getError() << std::endl;
            exit(EXIT_FAILURE);
        }

        try {
            std::string cfgListenAddr = cfg.lookup("listenAddress");
            listenAddr = cfgListenAddr;
            raftPort = cfg.lookup("raftPort");

            const libconfig::Setting& root = cfg.getRoot();
            const libconfig::Setting& servers = root["servers"];
            int numServers = servers.getLength();

            for (int i = 0; i < numServers; i++) {
                int serverId;
                std::string serverIPAddr;
                struct sockaddr_in serverSockAddr;

                const libconfig::Setting &server = servers[i];

                if (!server.lookupValue("id", serverId) ||
                    !server.lookupValue("address", serverIPAddr)) {
                        std::cerr << "Failed to read server " << i << 
                        " config information." << std::endl;
                        exit(EXIT_FAILURE);
                    }
                
                serverSockAddr.sin_family = AF_INET;
                serverSockAddr.sin_port = htons(raftPort);

                // Convert IPv4 and IPv6 addresses from text to binary
                // form
                if (inet_pton(AF_INET, serverIPAddr.c_str(),
                              &serverSockAddr.sin_addr) <= 0) {
                    std::cerr << "Invalid server config address" << 
                                 serverIPAddr << std::endl;
                    exit(EXIT_FAILURE);
                }

                /* Might need to do better memory management here e.g. not
                   a stack allocated string? */
                clusterMap[serverId] = serverSockAddr;
            }


        }
        catch(const libconfig::SettingNotFoundException &nfex)
        {
            std::cerr << "Server setting not found in cfg file," << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    bool Globals::addkQueueSocket(Socket* socket) {
        struct kevent ev;

        /* Set flags in event for kernel to notify when data arrives
           on socket and the udata pointer to the data identifier.  */
        EV_SET(&ev, socket->fd, EVFILT_READ, EV_ADD, 0, 0, socket);

        if (kevent(kq, &ev, 1, NULL, 0, NULL) == -1) {
            perror("Failure to register client socket");
            return false;
        }

        return true;
    }

    bool Globals::removekQueueSocket(Socket* socket) {
        struct kevent ev;

        /* Set flags for an event to stop the kernel from listening for
        events on this socket. */
        EV_SET(&ev, socket->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);

        if (kevent(kq, &ev, 1, NULL, 0, NULL) == -1) {
            perror("Failure to register client socket");
            return false;
        }

        delete socket;

        return true;
    }
}