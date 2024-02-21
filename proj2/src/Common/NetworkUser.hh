#ifndef COMMON_NETWORK_USER_H
#define COMMON_NETWORK_USER_H

#include <libconfig.h++>
#include <memory>
#include <string>
#include <unordered_map>

namespace Common {

/**
 * @brief Abstract class for a user of a NetworkService.
 *
 */
class NetworkUser {
 public:
  /**
   * @brief This method is overriden by the derived class. It is
   * the callback function for Network Service to pass on any messages
   * it receives to its NetworkUser
   *
   * @param receiveAddr The address from which the message was
   * received.
   * @param msg A message received by the Network Service.
   */
  virtual void handleNetworkMessage(const std::string& receiveAddr,
                                    const std::string& msg) = 0;

 protected:
  /**
   * @brief The callback function into the network service to send
   * a message on the network.
   * @param sendAddr network address to send message to
   * @param msg message to send
   */
  std::function<void(const std::string& sendAddr, const std::string& msg)>
      sendMsgFn;

};  // class NetworkUser
}  // namespace Common

#endif /* COMMON_NETWORK_USER_H */