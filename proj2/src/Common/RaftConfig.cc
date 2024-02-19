#include <string>
#include <libconfig.h++>
#include <iostream>
#include "RaftConfig.hh"

namespace Common {
    RaftConfig::RaftConfig( std::string configPath, RaftHostType type) {
        libconfig::Config cfg;

        try {
            cfg.readFile(configPath);

            if (type == SERVER) {
                std::string cfgServerAddr = cfg.lookup("serverAddress");
                std::cout << "[ServerConfig] Read serverAddr " << cfgServerAddr
                        << " from config." << std::endl;
                serverAddr = cfgServerAddr;

                serverId = cfg.lookup("serverId");

                std::string cfgPersistentStoragePath = 
                                    cfg.lookup("persistentStoragePath");
                
                persistentStoragePath = cfgPersistentStoragePath;
            }

            const libconfig::Setting& root = cfg.getRoot();
            const libconfig::Setting& servers = root["servers"];

            // Extract information about the servers in the Raft cluster
            for (int i = 0; i < servers.getLength(); i++) {
                const libconfig::Setting &server = servers[i];
                uint64_t serverId = server.lookup("id");
                std::string serverAddr = server.lookup("serverAddress");

                // Might need to be a non stack allocated string?
                clusterMap[serverId] = serverAddr;
            }

            // Number of servers including
            numClusterServers = servers.getLength() + 1;
        }
        catch (libconfig::FileIOException e) {
            std::cerr << "[ServerConfig] Bad path " << configPath << " provided"
                         "in command line." << std::endl;
            throw e;
        }
        catch (libconfig::SettingNotFoundException e) {
            std::cerr << "[ServerConfig] Setting not found: " << e.what()
                      << std::endl;
            throw e;
        }

    }

    RaftConfig::~RaftConfig()
    {
    }
}