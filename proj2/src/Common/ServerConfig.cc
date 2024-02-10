#include <string>
#include <netinet/in.h>
#include <libconfig.h++>
#include <iostream>
#include <arpa/inet.h>
#include "ServerConfig.hh"

namespace Common {
    ServerConfig::ServerConfig( std::string configPath ) {
        libconfig::Config cfg;

        cfg.readFile(configPath);

        std::string cfgListenAddr = cfg.lookup("listenAddress");
        listenAddr = cfgListenAddr;
        serverId = cfg.lookup("serverId");

        std::string cfgLogPath = cfg.lookup("logPath");
        logPath = cfgLogPath;

        const libconfig::Setting& root = cfg.getRoot();
        const libconfig::Setting& servers = root["servers"];

        // Extract information about the servers in the Raft cluster
        for (int i = 0; i < servers.getLength(); i++) {
            const libconfig::Setting &server = servers[i];
            uint64_t serverId = server.lookup("id");
            std::string serverAddr = server.lookup("address");

            // Might need to be a non stack allocated string?
            clusterMap[serverId] = serverAddr;
        }

        // Number of servers including
        numClusterServers = servers.getLength() + 1;

    }

    ServerConfig::~ServerConfig()
    {
    }
}