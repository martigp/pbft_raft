#include <string>
#include <netinet/in.h>
#include <libconfig.h++>
#include <iostream>
#include <arpa/inet.h>
#include "RaftClient/ClientConfig.hh"

namespace Raft {
    ClientConfig::ClientConfig( std::string configPath ) {
        libconfig::Config cfg;

        // Read the config file. Exit if any error.
        try
        {
            cfg.readFile(configPath);
        }
        catch(const libconfig::FileIOException &fioex)
        {
            std::cerr << "[ClientConfig]: I/O error while reading config. Exiting." << std::endl;
            exit(EXIT_FAILURE);
        }
        catch(const libconfig::ParseException &pex)
        {
            std::cerr << "[ClientConfig]: Config file parse error at " << pex.getFile() << ":" << pex.getLine()
                    << " - " << pex.getError() << std::endl;
            exit(EXIT_FAILURE);
        }

        try {
            std::string cfgAddr = cfg.lookup("addr");
            addr = cfgAddr;
            port = cfg.lookup("port");

            const libconfig::Setting& root = cfg.getRoot();
            const libconfig::Setting& servers = root["servers"];

            // Extract information about the servers in the Raft cluster
            for (int i = 0; i < servers.getLength(); i++) {
                uint64_t serverId;
                std::string serverIPAddr;
                uint64_t serverPort;

                const libconfig::Setting &server = servers[i];

                if (!server.lookupValue("id", serverId) ||
                    !server.lookupValue("address", serverIPAddr) ||
                    !server.lookupValue("port", serverPort)) {
                        std::cerr << "[ClientConfig]: Failed to read server " << i << 
                        " config information." << std::endl;
                        exit(EXIT_FAILURE);
                }

                // Might need to be a non stack allocated string?
                clusterMap[serverId] = {serverIPAddr, serverPort};
            }

            // Number of servers including
            numClusterServers = servers.getLength();

        }

        // Any error when parsing fields in the configuration file
        catch(const libconfig::SettingNotFoundException &nfex)
        {
            std::cerr << "[ClientConfig]: Server setting not found in cfg file," << std::endl;
            exit(EXIT_FAILURE);
        }

    }

    ClientConfig::~ClientConfig()
    {
    }
}