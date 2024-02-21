#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <iostream>
#include <chrono>
#include "Protobuf/RaftRPC.pb.h"
#include "RaftClient.hh"

namespace Raft {

    RaftClient::RaftClient()
        : config(CONFIG_PATH, Common::RaftHostType::CLIENT)
        , network( *this )
        , mostRecentRequestId (0)
        , receivedMessage (EMPTY_MSG)
        , receivedMessageLock()
        , receivedMessageCV()
    {
        // Set the current leader to first server in cluster map on startup.
        currentLeaderId = config.clusterMap.begin()->first;
    }

    RaftClient::~RaftClient()
    {}

    void 
    RaftClient::handleNetworkMessage(const std::string& sendAddr,
                                      const std::string& networkMsg) {
        receivedMessageLock.lock();
        receivedMessage = networkMsg;
        receivedMessageLock.unlock();

        receivedMessageCV.notify_all();
    }

    std::string
    RaftClient::sendToServer(std::string *cmd)
    {   
        // Iterate through each of the servers, send a message and wait on
        // a condition variable which has some timeout. Upon return of the
        // condition variable, parse message if populated otherwise try with
        // another server.
        while (true) {

            RPC_StateMachineCmd_Request * req = new RPC_StateMachineCmd_Request();

            std::string* tmpCmd = new std::string(*cmd);
            req->set_allocated_cmd(cmd);
            cmd = tmpCmd;

            req->set_requestid(mostRecentRequestId);
            mostRecentRequestId++;

            RPC rpc;
            rpc.set_allocated_statemachinecmdreq(req);

            std::string rpcString;
            if (!rpc.SerializeToString(&rpcString)) {
                return std::string("<Error> Failed to serialize malformed state"
                    " machine command: " + *cmd + ". Try again");
            }

            std::string serverAddr = config.clusterMap[currentLeaderId];
            std::cout << "[Client] Sending rpc " << rpc.DebugString()
                      << " to addr " << serverAddr << std::endl;
            
            network.sendMessage(serverAddr, rpcString, CREATE_CONNECTION);


            std::unique_lock<std::mutex> lock(receivedMessageLock);
            bool incrementLeaderId = true;
            while (true) {
                // If timeout and no received message, try sending again.
                if (receivedMessageCV.wait_for(lock, 
                        std::chrono::milliseconds(REQUEST_TIMEOUT), 
                        [&] { return receivedMessage != EMPTY_MSG; })) {
                    RPC_StateMachineCmd_Response resp;
                    if (!resp.ParseFromString(receivedMessage))
                        break;
                    
                    // Response to an old request, wait for more up to date
                    // response.
                    if (resp.requestid() != mostRecentRequestId) {
                        receivedMessage = EMPTY_MSG;
                        continue;
                    }

                    if (!resp.success()) {
                        currentLeaderId = resp.leaderid();
                        incrementLeaderId = false;
                        break;
                    }

                    delete cmd;
                    return resp.msg();
                }
                else {
                    break;
                }
            }
            lock.unlock();
            if (incrementLeaderId) {
                // If no hint just increment the currentLeaderId by 1, 
                // modulus because first leader id is 1
                currentLeaderId = 
                    (currentLeaderId % config.numClusterServers) + 1;
            }
        }
    }
}
