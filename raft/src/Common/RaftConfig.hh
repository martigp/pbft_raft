#ifndef COMMON_RAFTCONFIG_H
#define COMMON_RAFTCONFIG_H

#include <string>
#include <unordered_map>

namespace Common {

/**
 * @brief Distinguishes between a configuration for a RaftClient or RaftServer
 *
 */
enum RaftHostType { CLIENT, SERVER };

class RaftConfig {
 public:
  /**
   * @brief Constructor. Throws error if any issues opening or parsing the
   * configuration file.
   * @param configPath path to file with all Raft configuation
   * information.
   * @param type The type of RaftHost (either CLIENT or SERVER)
   */
  RaftConfig(std::string configPath, RaftHostType type);

  /* Destructor */
  ~RaftConfig();

  /**
   * @brief Address for RaftServer to listen for RPC requests on and
   * send corresponding RPC responses from. Addr in form ip:port. Used only
   * by a RaftServer
   */
  std::string serverAddr;

  /**
   * @brief Raft Server Id of this Raft Server. Used only by a RaftServer
   *
   */
  uint64_t serverId;

  /**
   * @brief The number of servers in the Raft Cluster.
   *
   */
  uint64_t numClusterServers;

  /**
   * @brief Maps Raft Server Id to two addresses in the form of
   * ip:port. The first address is the address to send RPC Requests to
   * and the second address is the address to send RPC Responses to
   * for that server.
   */
  std::unordered_map<uint64_t, std::string> clusterMap;

};  // class RaftConfig
}  // namespace Common

#endif /* COMMON_RAFTCONFIG_H */