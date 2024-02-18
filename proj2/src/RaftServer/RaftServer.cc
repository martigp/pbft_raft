#include <sys/event.h>
#include <libconfig.h++>
#include <iostream>
#include <cstdlib>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "RaftServer/RaftServer.hh"
#include "Protobuf/RaftRPC.pb.h"
#include <fstream>
#include <memory>
#include <functional>
#include <filesystem>

namespace Raft {
    
    RaftServer::RaftServer( const std::string& configPath, bool firstServerBoot)
        : config ( configPath )
        , storage( config.serverId, firstServerBoot )
        , network( *this )
        , eventQueueMutex()
        , eventQueueCV()
        , eventQueue()
        , myState ( RaftServer::ServerState::FOLLOWER )
        , commitIndex ( 0 )
        , leaderId ( 0 )
        , volatileServerInfo()
        , logToClientAddrMap( )
        , numVotesReceived ( 0 )
        , myVotes ( {} )
    {   
        timer.reset(new Timer(std::bind(&RaftServer::notifyRaftOfTimerEvent, this)));
        shellSM.reset(new ShellStateMachine(std::bind(&RaftServer::notifyRaftOfStateMachineApplied,
                      this, std::placeholders::_1, std::placeholders::_2)));
    }

    RaftServer::~RaftServer()
    {
    }

    void RaftServer::start()
    {
        // Set a random ElectionTimeout
        setRandomElectionTimeout();

        std::thread t(&Common::NetworkService::startListening, &network, config.serverAddr);
        t.detach();

        std::unique_lock<std::mutex> lock(eventQueueMutex);
        // Begin main loop waiting for events to be available on the event queue
        while (true) {
            while (eventQueue.empty()) {
                eventQueueCV.wait(lock);
            }

            RaftServerEvent nextEvent = eventQueue.front();
            eventQueue.pop();
            lock.unlock();
            printf("[RaftServer.cc]: Popped new event, queue length %zu\n", eventQueue.size());
            switch (nextEvent.type) {
                case EventType::TIMER_FIRED:
                    timeoutHandler();
                    break;
                case EventType::MESSAGE_RECEIVED:
                    processNetworkMessage(nextEvent.addr.value(), nextEvent.msg.value());
                    break;
                case EventType::STATE_MACHINE_APPLIED:
                    handleAppliedLogEntry(nextEvent.logIndex.value(), nextEvent.stateMachineResult.value());
                    break;
            }
            lock.lock();
        }
    }

    /*****************************************************
     * Publicly available callback methods for:
     * Network, Timer, StateMachine
    ******************************************************/

    void RaftServer::handleNetworkMessage(const std::string& sendAddr,
                                          const std::string& msg) {

        RaftServerEvent newEvent;
        newEvent.type = EventType::MESSAGE_RECEIVED;
        newEvent.addr = sendAddr;
        newEvent.msg = msg;
        std::cout << "[Raft server]: New network event received from " << 
        newEvent.addr.value() << std::endl;
        
        {
            std::lock_guard<std::mutex> lg(eventQueueMutex);
            eventQueue.push(newEvent);
        }

        eventQueueCV.notify_all();
        printf("[RaftServer.cc]: Network Message Handler: eventQueueCV notified, queue length %zu\n", eventQueue.size());
    }

    void RaftServer::notifyRaftOfTimerEvent() {
        RaftServerEvent newEvent;
        newEvent.type = EventType::TIMER_FIRED;

        {
            std::unique_lock<std::mutex> lock(eventQueueMutex);
            eventQueue.push(newEvent);
        }

        eventQueueCV.notify_all();
        printf("[RaftServer.cc]: Timer Event Handler: eventQueueCV notified, queue length %zu\n", eventQueue.size());
    }

    void RaftServer::notifyRaftOfStateMachineApplied(
                                         uint64_t logIndex,
                                         std::string* stateMachineResult) {
        RaftServerEvent newEvent;
        newEvent.type = EventType::STATE_MACHINE_APPLIED;
        newEvent.logIndex = logIndex;
        newEvent.stateMachineResult = stateMachineResult;

        {
            std::unique_lock<std::mutex> lock(eventQueueMutex);
            eventQueue.push(newEvent);
        }

        eventQueueCV.notify_all();
        printf("[RaftServer.cc]: SM Applied Handler: eventQueueCV notified, queue length %zu\n", eventQueue.size());
    }

    /*****************************************************
     * Internal methods for responding to RaftServerEvents
    ******************************************************/

    void RaftServer::timeoutHandler() {
        switch (myState) {
            case ServerState::FOLLOWER:
                printf("[RaftServer.cc]: Called timeout as follower\n");
                myState = ServerState::CANDIDATE;
                printf("[RaftServer.cc]: Converted to candidate\n");
                startNewElection();
                break;
            case ServerState::CANDIDATE:
                printf("[RaftServer.cc]: Called timeout as candidate\n");
                startNewElection();
                break;
            case ServerState::LEADER:
                printf("[RaftServer.cc]: Called timeout as leader\n");
                sendAppendEntriesReqs();
                break;
        }
    }

    void RaftServer::handleAppliedLogEntry(uint64_t appliedIndex,
                                           std::string* result) {
        // Update highest applied index
        // TODO: should we check this?
        storage.setLastAppliedValue(appliedIndex);

        auto clientAddrEntry = logToClientAddrMap.find(appliedIndex);
        if (clientAddrEntry != logToClientAddrMap.cend()) {
            RPC_StateMachineCmd_Response *resp = new RPC_StateMachineCmd_Response();
            resp->set_allocated_msg(result);
            RPC rpc;
            rpc.set_allocated_statemachinecmdresp(resp);
            network.sendMessage(clientAddrEntry->second, rpc.SerializeAsString());
            return;
        }
        //TODO: Need error handling here, what is the correct behaviour, does
        // this indicate a corruption that is fatal
    }

    void RaftServer::setRandomElectionTimeout() {
        std::random_device seed;
        std::mt19937 gen{seed()}; 
        std::uniform_int_distribution<> dist{5000, 10000};
        uint64_t timerTimeout = dist(gen);
        printf("[RaftServer.cc]: New timer timeout: %llu\n", timerTimeout);
        timer->resetTimer(timerTimeout);
    }

    void RaftServer::setHeartbeatTimeout() {
        uint64_t timerTimeout = 1000; // TODO: is it ok if this is hardcoded 
        printf("[RaftServer.cc]: New timer timeout: %llu\n", timerTimeout);
        timer->resetTimer(timerTimeout);
    }

    void RaftServer::startNewElection() {
        printf("[RaftServer.cc]: Start New Election\n");
        if (!storage.setCurrentTermValue(storage.getCurrentTermValue() + 1)) {
            std::cerr << "[RaftServer.cc]: Error while incrementing and writing current term value." << std::endl;
            exit(EXIT_FAILURE);
        }
        if (!storage.setVotedForValue(config.serverId)) {
            std::cerr << "[RaftServer.cc]: Error while writing votedFor value." << std::endl;
            exit(EXIT_FAILURE);
        }
        myVotes.clear();
        myVotes.insert(config.serverId);
        numVotesReceived = 1;
        timer->resetTimer();
        
        RPC_RequestVote_Request* req = new RPC_RequestVote_Request();
        req->set_term(storage.getCurrentTermValue());
        req->set_candidateid(config.serverId);
        req->set_lastlogindex(0);
        req->set_lastlogterm(0);

        RPC rpc;
        rpc.set_allocated_requestvotereq(req);

        std::string rpcString;
        
        if (!rpc.SerializeToString(&rpcString)) {
            std::cerr << "[Raft Server] Unable to serialize the Request Vote Request. " << std::endl;
            return;
        }


        // TODO: turn RPC's into strings for Network
        printf("[RaftServer.cc]: About to send RequestVote: term: %llu,"
               "serverId: %llu\n", storage.getCurrentTermValue(), config.serverId);
        for (auto& [_, serverAddr]: config.clusterMap) {
            network.sendMessage(serverAddr, rpcString,
                                CREATE_CONNECTION);
        }
    }

    void RaftServer::convertToFollower() {
        printf("[RaftServer.cc]: Converting to follower, term: %llu, serverId: %llu\n", storage.getCurrentTermValue(), config.serverId);
        myState = ServerState::FOLLOWER;
        setRandomElectionTimeout();
    }

    void RaftServer::convertToLeader() {
        printf("[RaftServer.cc]: Converting to leader, term: %llu, serverId: %llu\n", storage.getCurrentTermValue(), config.serverId);
        myState = ServerState::LEADER;
        setHeartbeatTimeout();
        // Reinitialize volatile state for leader
        for (auto& [raftServerId, sendToAddr]: config.clusterMap) {
            volatileServerInfo[raftServerId] = RaftServerVolatileState();
            /* Index of the next log entry to send to that server,
               initialized to last log index + 1*/
            volatileServerInfo[raftServerId].nextIndex = storage.getLogLength() + 1;
            /* Index of highest log entry known to be replicated on server,
               initialized to 0, increases monotonically*/
            volatileServerInfo[raftServerId].matchIndex = 0;
        }
        
        // Value of isHeartbeat set to true for first empty append entries requests
        sendAppendEntriesReqs(true);
    }

    void RaftServer::sendAppendEntriesReqs(std::optional<bool> isHeartbeat) {
        printf("[RaftServer.cc]: About to send AppendEntries, term: %llu\n", storage.getCurrentTermValue());
        for (auto& [raftServerId, sendToAddr]: config.clusterMap) {

            struct RaftServerVolatileState& serverInfo = 
                volatileServerInfo[raftServerId];

            RPC_AppendEntries_Request * req = new RPC_AppendEntries_Request();
            req->set_term(storage.getCurrentTermValue());
            req->set_leaderid(config.serverId);
            req->set_prevlogindex(serverInfo.nextIndex - 1);
            uint64_t term;
            std::string entry;

            if (serverInfo.nextIndex == 1) {
                req->set_prevlogterm(0); // TODO: if last index is 0(empty log), is 0 here ok?
            } else {
                if(!storage.getLogEntry(serverInfo.nextIndex - 1, term, entry)) {
                std::cerr << "[RaftServer.cc]: Error while reading log for sendAppendEntries at index " << std::to_string(serverInfo.nextIndex - 1) << std::endl;
                exit(EXIT_FAILURE);
                }
                req->set_prevlogterm(term);
            } 

            // TODO: this needs to append multiple entries if need
            // Wondering if we do a local cache or always access memory
            // RPC_AppendEntries_Request_Entry nullEntry;
            // *(req->add_entries()) = nullEntry;
            
            // avoid underflow of uint log indices by checking first if there's anythign to append
            // if (!isHeartbeat.has_value() && serverInfo.nextIndex <= storage.getLogLength()) {
            //     for (uint64_t i = 0; i <= storage.getLogLength() - serverInfo.nextIndex; i++) {
            //         RPC_AppendEntries_Request_Entry* entry = req->add_entries();
            //         uint64_t term;
            //         std::string entryCmd;
            //         if(!storage.getLogEntry(serverInfo.nextIndex + i, term, entryCmd)) {
            //             std::cerr << "[RaftServer.cc]: Error while reading log for sendAppendEntries at index " << std::to_string(serverInfo.nextIndex + i) << std::endl;
            //             exit(EXIT_FAILURE);
            //         }
            //         entry->set_cmd(entryCmd);
            //     }
            // }

            req->set_leadercommit(commitIndex);

            serverInfo.mostRecentRequestId++;
            req->set_requestid(serverInfo.mostRecentRequestId);

            RPC rpc;
            rpc.set_allocated_appendentriesreq(req);

            std::string rpcString;
            if (!rpc.SerializeToString(&rpcString)) {
                std::cerr << "[Raft Server] Unable to serialize the Append"
                " Entries Request to " << sendToAddr << std::endl;
                return;
            }
            printf("[RaftServer.cc]: Successfully serialized Append Entries Request to %s\n", sendToAddr.c_str());

            network.sendMessage(sendToAddr, rpcString, CREATE_CONNECTION);
        }
    }

    void RaftServer::processNetworkMessage(const std::string& senderAddr, 
                                                  const std::string& msg) {
        // Check it is a well formed Raft Message
        RPC rpc;
        if (!rpc.ParseFromString(msg)) {
            std::cerr << "[Server] Unable to parse message" << msg <<  
                         " from host " << senderAddr << std::endl;
            return;
        }
        RPC::MsgCase msgType = rpc.msg_case();

        switch(msgType) {
            case RPC::kAppendEntriesReq:
                processAppendEntriesReq(senderAddr,
                                        rpc.appendentriesreq());
                return;
            case RPC::kAppendEntriesResp:
                processAppendEntriesResp(senderAddr,
                                         rpc.appendentriesresp());
                return;
            case RPC::kRequestVoteReq:
                processRequestVoteReq(senderAddr,
                                      rpc.requestvotereq());
                return;
            case RPC::kRequestVoteResp:
                processRequestVoteResp(senderAddr,
                                       rpc.requestvoteresp());
                return;
            case RPC::kStateMachineCmdReq:
                processClientRequest(senderAddr, rpc.statemachinecmdreq());
                return;
            default:
                std::cerr << "[Server] Incorrectly received message of"
                << " type " << msgType << "from address " << senderAddr
                << std::endl;
                return;
        }
    }

    void RaftServer::processClientRequest(const std::string& clientAddr, 
                                          const RPC_StateMachineCmd_Request& req) {
        // Step 1: Append string cmd to log, get log index
        uint64_t nextLogIndex = storage.getLogLength() + 1;
        std::string entry;
        if (!storage.setLogEntry(nextLogIndex, storage.getCurrentTermValue(), entry)) {
            std::cerr << "[RaftServer.cc]: Error while writing entry to log." << std::endl;
            exit(EXIT_FAILURE);
        }

        // Step 2: Associate log index with addr to respond to
        logToClientAddrMap[nextLogIndex] = clientAddr;

        // This should be it?, now the AppendEntriesRPC calls will try to propogate the whole log
        // Receipt of responses will let us know when indices are committed. 
    }

    void RaftServer::processAppendEntriesReq(const std::string& senderAddr, 
                                             const RPC_AppendEntries_Request& req) {
        printf("[RaftServer.cc]: Received Append Entries\n");
        RPC_AppendEntries_Response* resp = new RPC_AppendEntries_Response();
        // If out of date, convert to follower before continuing
        if (req.term() > storage.getCurrentTermValue()) {
            storage.setCurrentTermValue(req.term());
            storage.setVotedForValue(0); // no vote casted in new term
            convertToFollower();
        }

        // Currently running an election, AppendEntries from new term leader, convert to follower and continue
        if (myState == ServerState::CANDIDATE && req.term() == storage.getCurrentTermValue()) {
            convertToFollower(); 
        }

        // Include requestID in response for RPC pairing
        resp->set_term(storage.getCurrentTermValue());
        resp->set_requestid(req.requestid());
        
        if (req.term() < storage.getCurrentTermValue()) {
            resp->set_success(false);
        } else {
            // At this point, we must be talking to the currentLeader, resetTimer as specified in Rules for Follower
            timer->resetTimer();

            // Update who the current leader is
            leaderId = req.leaderid();

            /** Skipped all of the log replication
             * Dropping soon in Project 2 :P
             * Leaving some space here as a mental marker :)
            */
            resp->set_success(true);
        }

        RPC rpc;
        rpc.set_allocated_appendentriesresp(resp);

        const std::string rpcString = rpc.SerializeAsString();
        network.sendMessage(senderAddr, rpcString);
    }

    void RaftServer::processAppendEntriesResp(const std::string& senderAddr,
                                              const RPC_AppendEntries_Response& resp) {
        printf("[RaftServer.cc]: Process Append Entries Response\n");

        // Obtain the ServerID from our map
        uint64_t rpcSenderId = 0;
        for (auto& [serverId, serverAddr] : config.clusterMap) {
            if (senderAddr == serverAddr) {
                rpcSenderId = serverId;
            }
        }

        if (rpcSenderId == 0) {
            std::cerr << "[Raft Server] Received an AppendEntries RPC response"
            "from addr " << senderAddr << " not associatd with a raft server"
            << std::endl;
            return;
        }

        // If out of date, convert to follower before continuing
        if (resp.term() > storage.getCurrentTermValue()) {
            storage.setCurrentTermValue(resp.term());
            storage.setVotedForValue(0); // no vote casted in new term
            convertToFollower();
        }

        // ignore if it is not the response to our most recent request
        uint64_t serverMostRecentRequestId = 
            volatileServerInfo[rpcSenderId].mostRecentRequestId;
        if (resp.requestid() != serverMostRecentRequestId) {
            return;
        }

        /** Skipped all of the log replication
         * Dropping soon in Project 2 :P
         * Leaving some space here as a mental marker :)
        */
    }

    void RaftServer::processRequestVoteReq(const std::string& senderAddr,
                                           const RPC_RequestVote_Request& req) {
        printf("[RaftServer.cc]: Received Request Vote\n");
        RPC_RequestVote_Response* resp = new RPC_RequestVote_Response();

        // If out of date, convert to follower before continuing
        if (req.term() > storage.getCurrentTermValue()) {
            storage.setCurrentTermValue(req.term());
            storage.setVotedForValue(0); // no vote casted in new term
            convertToFollower(); 
        }
        resp->set_term(storage.getCurrentTermValue());
        if (req.term() < storage.getCurrentTermValue()) {
            resp->set_votegranted(false);
        } else if (storage.getVotedForValue() == 0 || storage.getVotedForValue() == req.candidateid()) {
            std::cout << "[Raft Server] Voting for candidate " << 
            req.candidateid() << std::endl;
            
            storage.setVotedForValue(req.candidateid());
            resp->set_votegranted(true);
            timer->resetTimer();
        } else {
            resp->set_votegranted(false);
        }

        RPC rpc;
        rpc.set_allocated_requestvoteresp(resp);

        std::string rpcString = rpc.SerializeAsString();

        network.sendMessage(senderAddr, rpcString);
    }

    void RaftServer::processRequestVoteResp(const std::string& senderAddr,
                                            const RPC_RequestVote_Response& resp) {
        std::cout << "[RaftServer.cc]: Process Request Vote Response with term "
        << resp.term() << " my term is " << storage.getCurrentTermValue() << std::endl;

        uint64_t rpcSenderId = 0;
        for (auto& [serverId, serverAddr] : config.clusterMap) {
            if (senderAddr == serverAddr) {
                rpcSenderId = serverId;
            }
        }

        if (rpcSenderId == 0) {
            std::cerr << "[Raft Server] Received a Request Vote RPC response"
            "from addr " << senderAddr << " not associatd with a raft server"
            << std::endl;
            return;
        }

        // If out of date, convert to follower and return
        // TODO: can't happen here though?
        if (resp.term() > storage.getCurrentTermValue()) {
            storage.setCurrentTermValue(resp.term());
            storage.setVotedForValue(0); // no vote casted in new term
            convertToFollower();
            return;
        }

        if (resp.term() == storage.getCurrentTermValue() && myState == ServerState::CANDIDATE) {
            if (resp.votegranted() == true && myVotes.find(rpcSenderId) == myVotes.end()) {
                numVotesReceived += 1;
                myVotes.insert(rpcSenderId);
                std::cout << "[RaftServer] Received new valid vote num votes=" 
                << numVotesReceived << "threshold=" 
                << config.numClusterServers / 2 << std::endl;

                if (numVotesReceived > (config.clusterMap.size() / 2)) {
                    convertToLeader();
                }
            }
        }
    }
}