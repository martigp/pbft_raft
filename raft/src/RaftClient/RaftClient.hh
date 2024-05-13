#include <condition_variable>
#include <mutex>
#include <string>

#include "Common/NetworkService.hh"
#include "Common/RaftConfig.hh"
#include "Protobuf/RaftRPC.pb.h"

#define CONFIG_PATH "./config_client.cfg"

/* Timeout for a Raft Client Request. */
#define REQUEST_TIMEOUT 10000
/* Default value for receivedMessage to indicate no message received. */
#define EMPTY_MSG ""

namespace Raft {
class RaftClient : public Common::NetworkUser {
 public:
  /**
   * @brief Constructor. Throws error upon failure.
   * Requires config file at path specified by CONFIG_PATH
   */
  RaftClient();

  /* Destructor */
  ~RaftClient();

  /**
   * @brief Attempts to execute the state machine command on the Raft
   * Cluster. Blocks until the command was successfully executed or if
   * the cmd argument could not be serialized before sending to Raft
   * Cluster.
   *
   * @param cmd State machine command to execute
   *
   * @return std::string Result of attempt to execute command on
   * Raft Cluster. Either the result of the state machine command being
   * executed or an error message due to bad user input.
   */
  std::string sendToServer(std::string* cmd);

  /**
   * @brief Overriden NetworkUser callback function that is called by
   * the Network Service when it has received a message.
   *
   * @param sendAddr Host address from which a message was received
   * Formatted: "xx.xx.xx.xx:port"
   *
   * @param networkMsg Network message received
   */
  void handleNetworkMessage(const std::string& sendAddr,
                            const std::string& networkMsg);

 private:
  /**
   * @brief Configuration object constructed for a RaftClient
   */
  Common::RaftConfig config;

  /**
   * @brief The service used by the Raft Server to send and receive
   * messages on the network.
   */
  Common::NetworkService network;

  /**
   * @brief The serverId that the Raft Client sends requests to.
   *
   */
  uint64_t currentLeaderId;

  /**
   * @brief The unique ID of the most recent request the client has
   * sent.
   *
   */
  uint64_t mostRecentRequestId;

  /**
   * @brief The most recent message that has been received by the
   * the client.
   *
   */
  std::string receivedMessage;

  /**
   * @brief Mutual exclusion used to R/W to received Message
   *
   */
  std::mutex receivedMessageLock;

  /**
   * @brief Condition variable to signal to client that a new message
   * has been received by the server.
   *
   */
  std::condition_variable receivedMessageCV;
};
}  // namespace Raft