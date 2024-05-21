#include "HotStuffClient.hh"

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "Protobuf/RaftRPC.pb.h"

namespace Raft {

Client::Client()
    : config(CONFIG_PATH, Common::RaftHostType::CLIENT),
      network(*this),
      mostRecentRequestId(0),
      receivedMessage(EMPTY_MSG),
      receivedMessageLock(),
      receivedMessageCV() {
  // Set the current leader to first server in cluster map on startup.
  currentLeaderId = config.clusterMap.begin()->first;
  std::thread t(&Common::NetworkService::startListening, &network, "");
  t.detach();
}

Client::~Client() {}

void Client::handleNetworkMessage(const std::string& sendAddr,
                                      const std::string& networkMsg) {
  receivedMessageLock.lock();
  receivedMessage = networkMsg;
  std::cout << "[Client] Received a network message from server" << std::endl;
  receivedMessageLock.unlock();

  receivedMessageCV.notify_all();
}

std::string Client::sendToServer(std::string* cmd) {
  // Iterate through each of the servers, send a message and wait on
  // a condition variable which has some timeout. Upon return of the
  // condition variable, parse message if populated otherwise try with
  // another server.
  while (true) {
    RPC_StateMachineCmd_Request* req = new RPC_StateMachineCmd_Request();

    std::string* tmpCmd = new std::string(*cmd);
    req->set_allocated_cmd(cmd);
    cmd = tmpCmd;

    req->set_requestid(mostRecentRequestId);

    RPC rpc;
    rpc.set_allocated_statemachinecmdreq(req);

    std::string rpcString;
    if (!rpc.SerializeToString(&rpcString)) {
      return std::string(
          "<Error> Failed to serialize malformed state"
          " machine command: " +
          *cmd + ". Try again");
    }

    std::string serverAddr = config.clusterMap[currentLeaderId];

    network.sendMessage(serverAddr, rpcString, CREATE_CONNECTION);

    std::unique_lock<std::mutex> lock(receivedMessageLock);
    bool incrementLeaderId = true;
    while (true) {
      // If timeout and no received message, try sending again.
      if (receivedMessageCV.wait_for(
              lock, std::chrono::milliseconds(REQUEST_TIMEOUT),
              [&] { return receivedMessage != EMPTY_MSG; })) {
        RPC rpc;
        if (!rpc.ParseFromString(receivedMessage)) {
            std::cerr << "[Client] Unable to parse received RPC)" << std::endl;
            break;
        }

        if (rpc.msg_case() != RPC::kStateMachineCmdResp) {
          std::cerr << "[Client] RPC received wasn't StateMachine "
                       "Reponse."
                    << std::endl;
          break;
        }

        RPC_StateMachineCmd_Response resp = rpc.statemachinecmdresp();

        // Response to an old request, wait for more up to date
        // response.
        if (resp.requestid() != mostRecentRequestId) {
          std::cerr << "[Client] rejecting response because out "
                       " of date requestId Expected: "
                    << mostRecentRequestId << "Received " << resp.requestid()
                    << std::endl;
          receivedMessage = EMPTY_MSG;
          continue;
        }

        if (resp.success()) {
          delete cmd;
          return resp.msg();
        } else {
          std::cerr << "[Client] Server " << currentLeaderId
                    << " not the leader." << std::endl;
          if (currentLeaderId != 0) {
            // Indicates we have a hint for which server might be leader
            currentLeaderId = resp.leaderid();
            incrementLeaderId = false;
          }
          break;
        }
      } else {
          std::cout << "[Client] CV wait for evaluated as false w response: "
                    << receivedMessage << std::endl;
          break;
      }
    }
    lock.unlock();
    if (incrementLeaderId) {
      // If no hint as to which server is currently leader, just increment
      // the currentLeaderId by 1, modulus because first leader id is 1
      currentLeaderId = (currentLeaderId % config.numClusterServers) + 1;
    }

    mostRecentRequestId++;
  }
}
}  // namespace Raft