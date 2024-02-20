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

// Appropriate value chosen based off of Raft paper specifications
#define HEARTBEAT_TIMEOUT 1000

namespace Raft {
    
    RaftServer::RaftServer( const std::string& configPath, bool firstServerBoot)
        : config ( configPath, Common::RaftHostType::SERVER )
        , storage( config.serverId, firstServerBoot )
        , network( *this )
        , eventQueueMutex()
        , eventQueueCV()
        , eventQueue()
        , myState ( RaftServer::ServerState::FOLLOWER )
        , commitIndex ( 0 )
        , leaderId ( 0 )
        , volatileServerInfo()
        , logToClientRequestMap( )
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
                case RaftServerEvent::TIMER_FIRED:
                    timeoutHandler();
                    break;
                case RaftServerEvent::MESSAGE_RECEIVED:
                    processNetworkMessage(nextEvent.addr.value(), nextEvent.msg.value());
                    break;
                case RaftServerEvent::STATE_MACHINE_APPLIED:
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
        newEvent.type = RaftServerEvent::MESSAGE_RECEIVED;
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
        newEvent.type = RaftServerEvent::TIMER_FIRED;

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
        newEvent.type = RaftServerEvent::STATE_MACHINE_APPLIED;
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
                printf("[RaftServer.cc]: About to send AppendEntries, term: %llu\n", storage.getCurrentTermValue());
                for (auto& [raftServerId, sendToAddr]: config.clusterMap) {
                    sendAppendEntriesReq(raftServerId, sendToAddr);
                }
                break;
        }
    }

    void RaftServer::handleAppliedLogEntry(uint64_t appliedIndex,
                                           std::string* result) {
        // Update highest applied index
        // TODO: should we check this?
        storage.setLastAppliedValue(appliedIndex);

        auto clientRequestEntry = logToClientRequestMap.find(appliedIndex);
        if (clientRequestEntry != logToClientRequestMap.cend()) {
            RPC_StateMachineCmd_Response *resp = new RPC_StateMachineCmd_Response();

            uint64_t requestId = clientRequestEntry->second.first;
            resp->set_requestid(requestId);
            resp->set_allocated_msg(result);
            RPC rpc;
            rpc.set_allocated_statemachinecmdresp(resp);
            
            std::string clientAddr = clientRequestEntry->second.second;
            network.sendMessage(clientAddr, rpc.SerializeAsString());
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
        uint64_t timerTimeout = HEARTBEAT_TIMEOUT; 
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

        uint64_t logLen = storage.getLogLength();
        req->set_lastlogindex(logLen);
        if (logLen != 0) {
            uint64_t term;
            if (!storage.getLogEntry(logLen, term)) {
                std::cerr << "[RaftServer.cc]: Starting new election, error while getting term for last log entry." << std::endl;
                exit(EXIT_FAILURE);
            }
            req->set_lastlogterm(term);
        } else {
            req->set_lastlogterm(0);
        }

        RPC rpc;
        rpc.set_allocated_requestvotereq(req);

        std::string rpcString;
        
        if (!rpc.SerializeToString(&rpcString)) {
            std::cerr << "[Raft Server] Unable to serialize the Request Vote Request. " << std::endl;
            return;
        }

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
        
        // nextIndex is > loglength so no entries will get sent on the first set of requests
        printf("[RaftServer.cc]: About to send AppendEntries, term: %llu\n", storage.getCurrentTermValue());
        for (auto& [raftServerId, sendToAddr]: config.clusterMap) {
            sendAppendEntriesReq(raftServerId, sendToAddr);
        }
    }

    void RaftServer::sendAppendEntriesReq(uint64_t serverId, std::string serverAddr) {
        struct RaftServerVolatileState& serverInfo = 
            volatileServerInfo[serverId];

        RPC_AppendEntries_Request * req = new RPC_AppendEntries_Request();
        req->set_term(storage.getCurrentTermValue());
        req->set_leaderid(config.serverId);
        req->set_prevlogindex(serverInfo.nextIndex - 1);
        uint64_t term;
        std::string entry;

        if (serverInfo.nextIndex == 1) {
            // When NextIndex is 1, there are no previous log entries
            // and previous term should be set to 0 without attempting to access the log
            req->set_prevlogterm(0); 
        } else {
            if(!storage.getLogEntry(serverInfo.nextIndex - 1, term, entry)) {
                std::cerr << "[RaftServer.cc]: Error while reading log for sendAppendEntries at index " << std::to_string(serverInfo.nextIndex - 1) << std::endl;
                exit(EXIT_FAILURE);
            }
            req->set_prevlogterm(term);
        } 

        // TODO: Wondering if we do a local cache or always access memory
        
        // avoid underflow of uint log indices by checking first if there's anything to append
        if (serverInfo.nextIndex <= storage.getLogLength()) {
            for (uint64_t i = 0; i <= storage.getLogLength() - serverInfo.nextIndex; i++) {
                RPC_AppendEntries_Request_Entry* entry = req->add_entries();
                uint64_t term;
                std::string entryCmd;
                if(!storage.getLogEntry(serverInfo.nextIndex + i, term, entryCmd)) {
                    std::cerr << "[RaftServer.cc]: Error while reading log for sendAppendEntries at index " << std::to_string(serverInfo.nextIndex + i) << std::endl;
                    exit(EXIT_FAILURE);
                }
                entry->set_cmd(entryCmd);
                entry->set_term(term);
            }
        }

        req->set_leadercommit(commitIndex);

        serverInfo.mostRecentRequestId++;
        req->set_requestid(serverInfo.mostRecentRequestId);

        RPC rpc;
        rpc.set_allocated_appendentriesreq(req);

        std::string rpcString;
        if (!rpc.SerializeToString(&rpcString)) {
            std::cerr << "[Raft Server] Unable to serialize the Append"
            " Entries Request to " << serverAddr << std::endl;
            return;
        }
        
        network.sendMessage(serverAddr, rpcString, CREATE_CONNECTION);
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
        std::cout << "[RaftServer] Received Client Request " << req.DebugString() << std::endl;
        uint64_t nextLogIndex = storage.getLogLength() + 1;
        std::string entry;
        if (!storage.setLogEntry(nextLogIndex, storage.getCurrentTermValue(), entry)) {
            std::cerr << "[RaftServer.cc]: Error while writing entry to log." << std::endl;
            exit(EXIT_FAILURE);
        }

        // Step 2: Associate log index with request information to allow response
        logToClientRequestMap[nextLogIndex] = 
            std::make_pair(req.requestid(), clientAddr);

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
        
        // Raft Figure 2: Receiver Implementation
        // Step 1) outdated term check
        if (req.term() < storage.getCurrentTermValue()) { 
            resp->set_success(false);
        } else {
            // At this point, we must be talking to the currentLeader, resetTimer as specified in Rules for Follower
            timer->resetTimer();
            // Update who the current leader is
            leaderId = req.leaderid();

            // Step 2) Check entry at prevLogIndex
            // (only check prev if it is a valid log index)
            if (req.prevlogindex() != 0) {
                if (storage.getLogLength() >= req.prevlogindex()) {
                    uint64_t termAtIndex;
                    if (!storage.getLogEntry(req.prevlogindex(), termAtIndex)) {
                        std::cerr << "[RaftServer.cc]: Checking prevLogIndex in AppendEntriesRequest, error while getting term for log entry at index " << req.prevlogindex() << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    if (termAtIndex != req.prevlogterm()) {
                        resp->set_success(false); // mismatched term
                        goto sendAppendRPCResp;
                    }
                } else {
                    resp->set_success(false);  // log is shorter than previous log index
                    goto sendAppendRPCResp;
                }
            }
            /* Log must be matching up to prev log index at this point*/

            // Step 3) Check for conflicting entries
            uint64_t newFrom = req.entries_size() + 1; // could all be old
            for (uint64_t i = 1; i <= req.entries_size(); i++) {
                // entries starting at previous index + i must be new,
                // as they are longer than the current log
                if (storage.getLogLength() < req.prevlogindex() + i) {
                    newFrom = i;
                    break; // exit and start appending from i
                }
                uint64_t termAtIndex;
                if (!storage.getLogEntry(req.prevlogindex() + i, termAtIndex)) {
                    std::cerr << "[RaftServer.cc]: Checking for conflicting entries in AppendEntriesRequest, error while getting term for log entry at index " << req.prevlogindex() << std::endl;
                    exit(EXIT_FAILURE);
                }
                // conflicting entries starting at index (previous index + i)
                if (termAtIndex != req.entries(i-1).term()) {
                    storage.truncateLog(req.prevlogindex() + i);
                    newFrom = i;
                    break; // exit and start appending from i
                }
            }

            // Step 4) Append new entries not in the log
            for (uint64_t i = newFrom; i <= req.entries_size(); i++) {
                uint64_t index = req.prevlogindex() + i;
                if (!storage.setLogEntry(index, req.entries(i-1).term(), req.entries(i-1).cmd())) {
                    std::cerr << "[RaftServer.cc]: Error while setting entry at log index " << index << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            // Successful once this point is reached
            resp->set_success(true);

            // Step 5) compare leaderCommit to commit Index
            if (req.leadercommit() > commitIndex) {
                commitIndex = std::min(req.leadercommit(), storage.getLogLength());
            }
            // Rules for all servers: if commitIndex is > lastApplied,
            // apply to statemachine
            uint64_t lastApplied = storage.getLastAppliedValue();
            if (commitIndex > lastApplied) {
                for (uint64_t i = 1; i <= commitIndex - lastApplied; i++) {
                    uint64_t term;
                    std::string cmd;
                    if (!storage.getLogEntry(lastApplied + i, term, cmd)) {
                        std::cerr << "[RaftServer.cc]: Applying to SM in AppendEntries Request, error while getting log entry at index " << (lastApplied + i) << " to apply to state machine."<< std::endl;
                        exit(EXIT_FAILURE);
                    }
                    shellSM->pushCmd(lastApplied + i, cmd);
                }
            }
        }

        sendAppendRPCResp:
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

        // Ignore if it is not the response to our most recent request
        uint64_t serverMostRecentRequestId = 
            volatileServerInfo[rpcSenderId].mostRecentRequestId;
        if (resp.requestid() != serverMostRecentRequestId) {
            return;
        }

        // Update nextIndex/matchIndex, retry if needed
        if (resp.success()) {
            // Reinitialize nextIndex to lastIndex + 1
            volatileServerInfo[rpcSenderId].nextIndex = storage.getLogLength() + 1; 

            // Whole log must match, as we are always sending the maximum number of 
            // entries(whole log) with appendEntriesRequests
            volatileServerInfo[rpcSenderId].matchIndex = storage.getLogLength();

            // Check for commitIndex updates only on success
            uint64_t threshold = config.numClusterServers / 2;
            for (uint64_t N = commitIndex + 1; N <= storage.getLogLength(); N++) {
                // Gather running total of servers where matchIndex is >= N
                uint64_t matchIndexCount = 0; 
                for (auto& [serverId, serverAddr] : config.clusterMap) {
                    matchIndexCount += (volatileServerInfo[serverId].matchIndex >= N);
                }
                uint64_t entryTerm;
                if (!storage.getLogEntry(N, entryTerm)) {
                    std::cerr << "[RaftServer.cc]: Updating commitIndex, error while getting term for log entry at index " << N << std::endl;
                    exit(EXIT_FAILURE);
                }
                if (matchIndexCount > threshold && entryTerm == storage.getCurrentTermValue()) {
                    commitIndex = N;
                }
            }
        } else {
            // Decrement nextIndex but don't let it fall below 1
            if (volatileServerInfo[rpcSenderId].nextIndex != 1) {
                volatileServerInfo[rpcSenderId].nextIndex--;
            }
            // Retry now that nextIndex has been decremented
            sendAppendEntriesReq(rpcSenderId, senderAddr);
        }

        // Rules for all servers: if commitIndex is > lastApplied,
        // apply to statemachine
        uint64_t lastApplied = storage.getLastAppliedValue();
        if (commitIndex > lastApplied) {
            for (uint64_t i = 1; i <= commitIndex - lastApplied; i++) {
                uint64_t term;
                std::string cmd;
                if (!storage.getLogEntry(lastApplied + i, term, cmd)) {
                    std::cerr << "[RaftServer.cc]: Applying to SM in processAppendEntriesResp, error while getting log entry at index " << (lastApplied + i) << " to apply to state machine."<< std::endl;
                    exit(EXIT_FAILURE);
                }
                shellSM->pushCmd(lastApplied + i, cmd);
            }
        }
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
        } else if (storage.getVotedForValue() == 0 || storage.getVotedForValue() == req.candidateid()) { // check if you've voted for someone else
            // Check if log is more up to date:
            uint64_t myLastTerm;
            uint64_t candLastTerm = req.lastlogterm();

            // Acquire myLastTerm, setting to 0 if log is empty
            if (storage.getLogLength() != 0) {
                if (!storage.getLogEntry(storage.getLogLength(), myLastTerm)) {
                    std::cerr << "[RaftServer.cc]: While respond to RequestVote, error while getting term for log entry at index " << storage.getLogLength() << std::endl;
                    exit(EXIT_FAILURE);
                }
            } else {
                myLastTerm = 0;
            }

            if ((myLastTerm > candLastTerm) || 
                ((myLastTerm == candLastTerm) && (storage.getLogLength() > req.lastlogindex()))) {
                    std::cout << "[Raft Server] Log more up to date, rejecting vote for candidate " 
                              << req.candidateid() << std::endl;
                    resp->set_votegranted(false);
            } else {
                std::cout << "[Raft Server] Voting for candidate " << 
                req.candidateid() << std::endl;
                
                storage.setVotedForValue(req.candidateid());
                resp->set_votegranted(true);
                timer->resetTimer();
            }
        } else {
            std::cout << "[Raft Server] Already voted for " << storage.getVotedForValue() <<
                         ", rejecting vote for candidate " << req.candidateid() << std::endl;
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