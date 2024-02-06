#include <string>
#include <netinet/in.h>
#include <libconfig.h++>
#include <iostream>
#include <arpa/inet.h>
#include "NetworkServiceConfig.hh"

namespace Common {
    NetworkServiceConfig::NetworkServiceConfig( std::string configPath ) {
        libconfig::Config cfg;

        // Read the config file. Exit if any error.
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
            networkPort = cfg.lookup("raftPort");
            serverId = cfg.lookup("serverId");

            std::string cfgLogPath = cfg.lookup("logPath");
            logPath = cfgLogPath;

            const libconfig::Setting& root = cfg.getRoot();
            const libconfig::Setting& servers = root["servers"];

            // Extract information about the servers in the Raft cluster
            for (int i = 0; i < servers.getLength(); i++) {
                uint64_t serverId;
                std::string serverIPAddr;
                uint64_t serverPort;
                struct sockaddr_in serverSockAddr;


                const libconfig::Setting &server = servers[i];

                if (!server.lookupValue("id", serverId) ||
                    !server.lookupValue("address", serverIPAddr) ||
                    !server.lookupValue("port", serverPort)) {
                        std::cerr << "Failed to read server " << i << 
                        " config information." << std::endl;
                        exit(EXIT_FAILURE);
                }
                
                serverSockAddr.sin_family = AF_INET;
                serverSockAddr.sin_port = htons((uint16_t)serverPort);

                // Populate the serverSockAddr with an IP address. Now ready
                // for use with any socket functions.
                if (inet_pton(AF_INET, serverIPAddr.c_str(),
                              &serverSockAddr.sin_addr) <= 0) {
                    std::cerr << "Invalid server config address" << 
                                 serverIPAddr << std::endl;
                    exit(EXIT_FAILURE);
                }

                // Might need to be a non stack allocated string?
                clusterMap[serverId] = serverSockAddr;
            }

            // Number of servers including
            numClusterServers = servers.getLength() + 1;


        }

        // Any error when parsing fields in the configuration file
        catch(const libconfig::SettingNotFoundException &nfex)
        {
            std::cerr << "Server setting not found in cfg file," << std::endl;
            exit(EXIT_FAILURE);
        }

    }

    NetworkServiceConfig::~NetworkServiceConfig()
    {
    }
}