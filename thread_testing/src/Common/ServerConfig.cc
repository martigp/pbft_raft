#include <string>
#include <netinet/in.h>
#include <libconfig.h++>
#include <iostream>
#include <arpa/inet.h>
#include "ServerConfig.hh"

namespace Common {
    ServerConfig::ServerConfig( std::string configPath ) {
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
            serverID = cfg.lookup("serverID");
            std::string cfgListenAddr = cfg.lookup("listenAddress");
            listenAddr = cfgListenAddr;
            raftPort = cfg.lookup("raftPort");

            const libconfig::Setting& root = cfg.getRoot();
            const libconfig::Setting& servers = root["servers"];
            int numServers = servers.getLength();

            // Extract information about the servers in the Raft cluster
            for (int i = 0; i < numServers; i++) {
                uint64_t serverId;
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
        }

        // Any error when parsing fields in the configuration file
        catch(const libconfig::SettingNotFoundException &nfex)
        {
            std::cerr << "Server setting not found in cfg file," << std::endl;
            exit(EXIT_FAILURE);
        }

    }

    ServerConfig::~ServerConfig()
    {
    }
}