#include "RaftConfig.hh"

#include <iostream>
#include <libconfig.h++>
#include <string>

namespace Common {
RaftConfig::RaftConfig(std::string configPath, RaftHostType type) {
  libconfig::Config cfg;

  try {
    cfg.readFile(configPath);

    if (type == SERVER) {
      // Server specific configuation information
      std::string cfgServerAddr = cfg.lookup("serverAddress");
      serverAddr = cfgServerAddr;

      serverId = cfg.lookup("serverId");
    }

    const libconfig::Setting& root = cfg.getRoot();
    const libconfig::Setting& servers = root["servers"];

    // Extract information about the servers in the Raft cluster
    for (int i = 0; i < servers.getLength(); i++) {
      const libconfig::Setting& server = servers[i];
      uint64_t serverId = server.lookup("id");
      std::string serverAddr = server.lookup("address");

      clusterMap[serverId] = serverAddr;
    }

    // Number of servers in Raft Cluster
    if (type == CLIENT) {
      numClusterServers = clusterMap.size();
    } else {
      // Include self
      numClusterServers = clusterMap.size() + 1;
    }
  } catch (libconfig::FileIOException e) {
    std::cerr << "[ServerConfig] Bad path: " << configPath << " does not exist."
              << std::endl;
    throw e;
  } catch (libconfig::SettingNotFoundException e) {
    std::cerr << "[ServerConfig] Setting not found: " << e.what() << std::endl;
    throw e;
  }
}

RaftConfig::~RaftConfig() {}
}  // namespace Common