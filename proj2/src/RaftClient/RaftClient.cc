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

    // TODO: properly decompose this
    std::string
    RaftClient::connectAndSendToServer(std::string *in)
    {   
        // Iterate through each of the servers, send a message and wait on
        // a condition variable which has some timeout. Upon return of the
        // condition variable, parse message if populated otherwise try with
        // another server.
        while (true) {

            RPC_StateMachineCmd_Request * req = new RPC_StateMachineCmd_Request();
            req->set_allocated_cmd(in);
            
            req->set_requestid(mostRecentRequestId);
            mostRecentRequestId++;

            RPC rpc;
            rpc.set_allocated_statemachinecmdreq(req);

            std::string rpcString;
            if (!rpc.SerializeToString(&rpcString)) {
                return std::string("<Error> Failed to serialize malformed state"
                    " machine command: " + *in + ". Try again");
            }

            std::string serverAddr = config.clusterMap[currentLeaderId];
            network.sendMessage(serverAddr, rpcString, CREATE_CONNECTION);

            std::cout << "<2>" <<  std::endl;

            std::unique_lock<std::mutex> lock(receivedMessageLock);
            bool incrementLeaderId = true;
            std::cout << "<3>" <<  std::endl;
            while (true) {
                // If timeout and no received message, try sending again.
                if (receivedMessageCV.wait_for(lock, 
                        std::chrono::milliseconds(REQUEST_TIMEOUT), 
                        [&] { return receivedMessage != EMPTY_MSG; })) {
                    std::cout << "<4.1>" <<  std::endl;
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

                    return resp.msg();
                }
                else {
                    std::cout << "<4.2>" <<  std::endl;
                    break;
                }
            }
            std::cout << "<5>" <<  std::endl;
            lock.unlock();
            std::cout << "<6>" <<  std::endl;
            if (incrementLeaderId) {
                // If not hint just increment the currentLeaderId by 1, 
                // modulus because first leader id is 1
                currentLeaderId = 
                    (currentLeaderId % config.numClusterServers) + 1;
            }
            std::cout << "<7>" <<  std::endl;
        }
    }
}
