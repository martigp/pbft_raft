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
        , shellSM ( this )
        , timer ( this )
        , storage ( config.persistentStoragePath, firstServerBoot )
        , network( *this )
        , eventQueueMutex()
        , eventQueueCV()
        , eventQueue()
        , myState ( RaftServer::ServerState::FOLLOWER )
        , currentTerm ( 0 )
        , votedFor ( 0 )
        , lastApplied ( 0 )
        , commitIndex ( 0 )
        , leaderId ( 0 )
        , volatileServerInfo()
        , logToClientAddrMap( )
        , numVotesReceived ( 0 )
        , myVotes ( {} )
    {   
    }

    RaftServer::~RaftServer()
    {
    }

    void RaftServer::start()
    {
        // Set a random ElectionTimeout
        setRandomElectionTimeout();

        network.startListening(config.serverAddr);

        std::unique_lock<std::mutex> lock(eventQueueMutex);
        // Begin main loop waiting for events to be available on the event queue
        while (true) {
            while (eventQueue.empty()) {
                eventQueueCV.wait(lock);
            }

            RaftServerEvent nextEvent = eventQueue.front();
            eventQueue.pop();
            lock.unlock();

            switch (nextEvent.type) {
                case EventType::TIMER_FIRED:
                    timeoutHandler();
                case EventType::MESSAGE_RECEIVED:
                    processNetworkMessage(nextEvent.addr.value(),
                                                 nextEvent.msg.value());
                case EventType::STATE_MACHINE_APPLIED:
                    handleAppliedLogEntry(nextEvent.logIndex.value(), nextEvent.stateMachineResult.value());
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
        
        {
            std::lock_guard<std::mutex> lg(eventQueueMutex);
            eventQueue.push(newEvent);
        }

        eventQueueCV.notify_all();
    }

    void RaftServer::notifyRaftOfTimerEvent() {
        RaftServerEvent newEvent;
        newEvent.type = EventType::TIMER_FIRED;

        {
            std::unique_lock<std::mutex> lock(eventQueueMutex);
            eventQueue.push(newEvent);
        }

        eventQueueCV.notify_all();
    }

    void RaftServer::notifyRaftOfStateMachineApplied(
                                         uint64_t logIndex,
                                         const std::string stateMachineResult) {
        RaftServerEvent newEvent;
        newEvent.type = EventType::STATE_MACHINE_APPLIED;
        newEvent.logIndex = logIndex;
        newEvent.stateMachineResult = stateMachineResult;

        {
            std::unique_lock<std::mutex> lock(eventQueueMutex);
            eventQueue.push(newEvent);
        }

        eventQueueCV.notify_all();
    }

    /*****************************************************
     * Internal methods for responding to RaftServerEvents
    ******************************************************/

    void RaftServer::timeoutHandler() {
        switch (myState) {
            case ServerState::FOLLOWER:
                myState = ServerState::CANDIDATE;
                startNewElection();
            case ServerState::CANDIDATE:
                startNewElection();
            case ServerState::LEADER:
                sendAppendEntriesReqs();
        }
    }

    void RaftServer::handleAppliedLogEntry(uint64_t appliedIndex,
                                           const std::string& result) {
        // Update highest applied index
        // TODO: should we check this?
        lastApplied = appliedIndex;

        auto clientAddrEntry = logToClientAddrMap.find(appliedIndex);
        if (clientAddrEntry != logToClientAddrMap.cend()) {
            network.sendMessage(clientAddrEntry->second, result);
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
        timer.resetTimer(timerTimeout);
    }

    void RaftServer::setHeartbeatTimeout() {
        uint64_t timerTimeout = 1000; // TODO: is it ok if this is hardcoded 
        printf("[RaftServer.cc]: New timer timeout: %llu\n", timerTimeout);
        timer.resetTimer(timerTimeout);
    }

    void RaftServer::startNewElection() {
        printf("[RaftServer.cc]: Start New Election\n");
        currentTerm += 1;
        votedFor = config.serverId;
        writePersistentState(); // TODO: add real persistent state
        myVotes.clear();
        myVotes.insert(config.serverId);
        numVotesReceived = 1;
        timer.resetTimer();

        RPC_RequestVote_Request req;
        req.set_term(currentTerm);
        req.set_candidateid(config.serverId);
        req.set_lastlogindex(0);
        req.set_lastlogterm(0);
        std::string reqString = req.SerializeAsString();

        // TODO: turn RPC's into strings for Network
        printf("[RaftServer.cc]: About to send RequestVote, term: %d, serverId: %llu\n", currentTerm, config.serverId);
        for (auto& [_, raftServerAddrs]: config.clusterMap) {
            network.sendMessage(raftServerAddrs.first, reqString);
        }
    }

    void RaftServer::sendAppendEntriesReqs(std::optional<bool> isHeartbeat) {
        RPC_AppendEntries_Request req;
        printf("[RaftServer.cc]: About to send AppendEntries, term: %d\n", currentTerm);
        for (auto& [raftServerId, raftServerAddrs]: config.clusterMap) {
            // NOTE: sendMessage only requires an addr, port, string

            // Send request to the RaftServer server address.
            const std::string sendToAddr = raftServerAddrs.second;

            struct RaftServerVolatileState& serverInfo = 
                volatileServerInfo[raftServerId];

            req.set_term(currentTerm);
            req.set_leaderid(config.serverId);
            req.set_prevlogindex(serverInfo.nextIndex - 1);
            uint64_t term;
            std::string entry;
            if(!storage.getLogEntry(serverInfo.nextIndex - 1, term, entry)) {
                std::cerr << "[RaftServer.cc]: Error while reading log for sendAppendEntries." << std::endl;
                exit(EXIT_FAILURE);
            }
            req.set_prevlogterm(term);

            // TODO: this needs to append multiple entries if need
            // Wondering if we do a local cache or always access memory
            RPC_AppendEntries_Request_Entry nullEntry;
            if (isHeartbeat.value()) {
                *(req.add_entries()) = nullEntry;
            } else {
                // Some for loop with the same logic as above
                *(req.add_entries()) = nullEntry;
            }

            // TODO: go over the most recent request_id stuff
            // Check that getting this refernce actually workss
            serverInfo.mostRecentRequestId++;

            req.set_requestid(serverInfo.mostRecentRequestId);

            std::string reqString = req.SerializeAsString();
            network.sendMessage(sendToAddr, reqString, CREATE_CONNECTION,
                                config.clientAddr);
        }
    }

    void RaftServer::processNetworkMessage(const std::string& senderAddr, 
                                                  const std::string& msg) {
        // Check it is a well formed Raft Message
        RPC rpc;
        if (!rpc.ParseFromString(msg)) {
            return;
        }
        RPC::MsgCase msgType = rpc.msg_case();

        // Option 1: Message is a Raft Server, or use a .find method idk
        for (auto& [raftServerId, raftServerAddrs]: config.clusterMap) {
            // First is the address for the connection where the Raft Server
            // acts as a client i.e. the message should be a request
            if (raftServerAddrs.first == senderAddr) {
                 // TODO: figure out how we will parse strings with one of
                switch (msgType) {
                    case RPC::kAppendEntriesReq:
                        processAppendEntriesReq(raftServerId,
                                                rpc.appendentriesreq());
                        return;
                    case RPC::kRequestVoteReq:
                        processRequestVoteReq(raftServerId,
                                              rpc.requestvotereq());
                        return;
                    default:
                        return;
                }
            }
            // Second address is where the Raft Server acts as a server i.e.
            // the message should be a response.
            else if (raftServerAddrs.second == senderAddr) {
                switch(msgType) {
                    case RPC::kAppendEntriesResp:
                        processAppendEntriesResp(raftServerId,
                                                 rpc.appendentriesresp());
                        return;
                    case RPC::kRequestVoteResp:
                        processRequestVoteResp(raftServerId,
                                               rpc.requestvoteresp());
                        return;
                    default:
                        return;
                    
                }
            }
        }

        // Option 2: Did not find a raftServer matching address, check that
        // the message is a State Machine Command Request. Do nothing if not.
        if (msgType == RPC::kStateMachineCmdReq) {
            processClientRequest(senderAddr, rpc.statemachinecmdreq());
        }
    }

    void RaftServer::processClientRequest(const std::string& clientAddr, 
                                          const RPC_StateMachineCmd_Request& req) {
        // Step 1: Append string cmd to log, get log index
        uint64_t nextLogIndex = storage.getLogLength() + 1;
        std::string entry;
        if (!storage.setLogEntry(nextLogIndex, currentTerm, entry)) {
            std::cerr << "[RaftServer.cc]: Error while writing entry to log." << std::endl;
            exit(EXIT_FAILURE);
        }

        // Step 2: Associate log index with addr to respond to
        logToClientAddrMap[nextLogIndex] = clientAddr;

        // This should be it?, now the AppendEntriesRPC calls will try to propogate the whole log
        // Receipt of responses will let us know when indices are committed. 
    }

    void RaftServer::processAppendEntriesReq(uint64_t serverId, 
                                             const RPC_AppendEntries_Request& req) {
        printf("[RaftServer.cc]: Received Append Entries\n");
        RPC_AppendEntries_Response resp;
        // If out of date, convert to follower before continuing
        if (req.term() > currentTerm) {
            currentTerm = req.term();
            votedFor = 0; // no vote casted in new term
            convertToFollower();
        }

        // Currently running an election, AppendEntries from new term leader, convert to follower and continue
        if (myState == ServerState::CANDIDATE && req.term() == currentTerm) {
            convertToFollower(); 
        }

        // Include requestID in response for RPC pairing
        resp.set_term(currentTerm);
        resp.set_requestid(req.requestid());
        
        if (req.term() < currentTerm) {
            resp.set_success(false);
        } else {
            // At this point, we must be talking to the currentLeader, resetTimer as specified in Rules for Follower
            timer.resetTimer();

            // Update who the current leader is
            leaderId = req.leaderid();

            /** Skipped all of the log replication
             * Dropping soon in Project 2 :P
             * Leaving some space here as a mental marker :)
            */
            resp.set_success(true);
        }

        const std::string respString = resp.SerializeAsString();

        auto serverAddrs = config.clusterMap[serverId];
        network.sendMessage(serverAddrs.first, respString);
    }

    void RaftServer::processAppendEntriesResp(uint64_t serverId,
                                              const RPC_AppendEntries_Response& resp) {
        printf("[RaftServer.cc]: Process Append Entries Response\n");
        // If out of date, convert to follower before continuing
        if (resp.term() > currentTerm) {
            currentTerm = resp.term();
            votedFor = 0; // no vote casted in new term
            convertToFollower();
        }
        /* Do we do anything here for project 1? */
        // ignore if it is not the response to our msot recent request
        uint64_t serverMostRecentRequestId = 
            volatileServerInfo[serverId].mostRecentRequestId;
        if (resp.requestid() != serverMostRecentRequestId) {
            return;
        }

        /** Skipped all of the log replication
         * Dropping soon in Project 2 :P
         * Leaving some space here as a mental marker :)
        */
    }

    void RaftServer::processRequestVoteReq(uint64_t serverId,
                                           const RPC_RequestVote_Request& req) {
        printf("[RaftServer.cc]: Received Request Vote\n");
        RPC::RequestVote::Response resp;

        // If out of date, convert to follower before continuing
        if (req.term() > currentTerm) {
            currentTerm = req.term();
            votedFor = 0; // no vote casted in new term
            convertToFollower(); 
        }

        resp.set_term(currentTerm);
        if (req.term() < currentTerm) {
            resp.set_votegranted(false);
        } else if (votedFor == 0 || votedFor == req.candidateid()) {
            votedFor = req.candidateid();
            resp.set_votegranted(true);
            timer.resetTimer();
        } else {
            resp.set_votegranted(false);
        }
        // TODO: send the string not the RPC
        std::string respString = resp.SerializeAsString();

        auto serverAddrs = config.clusterMap[serverId];
        network.sendMessage(serverAddrs.first, respString);
    }

    void RaftServer::processRequestVoteResp(uint64_t serverId,
                                            const RPC_RequestVote_Response& resp) {
        printf("[RaftServer.cc]: Process Request Vote Response\n");
        // If out of date, convert to follower and return
        // TODO: can't happen here though?
        if (resp.term() > currentTerm) {
            currentTerm = resp.term();
            votedFor = 0; // no vote casted in new term
            convertToFollower();
            return;
        }
        if (resp.term() == currentTerm && myState == ServerState::CANDIDATE) {
            if (resp.votegranted() == true && myVotes.find(serverId) == myVotes.end()) {
                numVotesReceived += 1;
                myVotes.insert(serverId);
                if (numVotesReceived > (config.clusterMap.size() / 2)) {
                    convertToLeader();
                }
            }
        }
    }
}