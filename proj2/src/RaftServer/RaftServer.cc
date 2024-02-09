#include <sys/event.h>
#include <libconfig.h++>
#include <iostream>
#include <cstdlib>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "RaftServer/RaftServer.hh"
#include <fstream>
#include <filesystem>

namespace Raft {
    
    RaftServer::RaftServer( std::string configPath, uint64_t serverID )
        : network( )
        , timer( )
        , storage( )
        , myState ( RaftServer::ServerState::FOLLOWER )
        , serverId ( config.serverId )
        , leaderId ( 0 )
        , currentTerm ( 0 )
        , votedFor ( 0 )
        , commitIndex ( 0 )
        , lastApplied ( 0 )
        , nextIndex ( 0 )
        , matchIndex ( {} )
        , mostRecentRequestId ( 0 )
        , logToClientIPMap( {} )
        , numVotesReceived ( 0 )
        , myVotes ( {} )
    {   
        config = ServerConfig(configPath, serverID);  
        try {  
            timer.reset(new Timer(&RaftServer::notifyRaftOfTimerEvent, *this));
            shellSM.reset(new ShellStateMachine(&RaftServer::notifyRaftOfStateMachineApplied, *this)); 
            network.reset(new network(&RaftServer::notifyRaftOfNetworkMessage, *this)); 
        } catch(const std::exception& e) {
            std::cerr << e.what() << std::endl;
        } 
    }

    RaftServer::~RaftServer()
    {
    }

    void RaftServer::start()
    {
        // Set a random ElectionTimeout
        generateRandomElectionTimeout();

        // Begin main loop waiting for events to be available on the event queue
        while (true) {
            std::unique_lock<std::mutex> lock(eventQueueMutex);
            while (eventQueue.empty()) {
                eventQueueCV.wait(lock);
            }

            RaftServerEvent nextEvent = eventQueue.front();
            eventQueue.pop();

            switch (nextEvent.type) {
                case EventType::TIMER_FIRED:
                    timeoutHandler();
                case EventType::MESSAGE_RECEIVED:
                    parseAndHandleNetworkMessage(nextEvent.ipAddr.value(), nextEvent.networkMsg.value());
                case EventType::STATE_MACHINE_APPLIED:
                    handleAppliedLogEntry(nextEvent.logIndex.value(), nextEvent.stateMachineResult.value());
            }
        }
    }

    /*****************************************************
     * Publicly available callback methods for:
     * Network, Timer, StateMachine
    ******************************************************/

    void notifyRaftOfNetworkMessage(std::string ipAddr, std::string networkMsg) {
        std::unique_lock<std::mutex> lock(eventQueueMutex);
        RaftServerEvent newEvent;
        newEvent.type = EventType::MESSAGE_RECEIVED;
        newEvent.ipAddr = ipAddr;
        eventQueue.push(newEvent);
        eventQueueCV.notify_all();
    }

    void notifyRaftOfTimerEvent() {
        std::unique_lock<std::mutex> lock(eventQueueMutex);
        RaftServerEvent newEvent;
        newEvent.type = EventType::TIMER_FIRED;
        eventQueue.push(newEvent);
        eventQueueCV.notify_all();
    }

    void notifyRaftOfStateMachineApplied(uint64_t logIndex, std::string stateMachineResult) {
        std::unique_lock<std::mutex> lock(eventQueueMutex);
        RaftServerEvent newEvent;
        newEvent.type = EventType::STATE_MACHINE_APPLIED;
        newEvent.logIndex = logIndex;
        newEvent.stateMachineResult = stateMachineResult;
        eventQueue.push(newEvent);
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
                sendAppendEntriesRPCs();
        }
    }

    void RaftServer::handleAppliedLogEntry(uint64_t appliedIndex, std::string result) {
        // Update highest applied index
        // TODO: should we check this?
        lastApplied = appliedIndex;

        // Check if a RaftClient is waiting on the result
        if (logToClientIPMap.contains(appliedIndex)) {
            network->sendMessage(logToClientIPMap[appliedIndex], result);
        }
    }

    void RaftServer::generateRandomElectionTimeout() {
        std::random_device seed;
        std::mt19937 gen{seed()}; 
        std::uniform_int_distribution<> dist{5000, 10000};
        uint64_t timerTimeout = dist(gen);
        printf("[RaftServer.cc]: New timer timeout: %llu\n", timerTimeout);\
        timer->resetTimer(timerTimeout);
    }

    void RaftServer::setHeartbeatTimeout() {
        uint64_t timerTimeout = 1000; // TODO: is it ok if this is hardcoded 
        printf("[RaftServer.cc]: New timer timeout: %llu\n", timerTimeout);\
        timer->resetTimer(timerTimeout);
    }

    void RaftServer::startNewElection() {
        printf("[RaftServer.cc]: Start New Election\n");
        currentTerm += 1;
        votedFor = config.serverId;
        writePersistentState(); // TODO: add real persistent state
        myVotes.clear();
        myVotes.insert(config.serverId);
        numVotesReceived = 1;
        resetTimer();

        Raft::RPC::RequestVote::Request req;
        req.set_term(currentTerm);
        req.set_candidateid(config.serverId);
        req.set_lastlogindex(0);
        req.set_lastlogterm(0);
        std::string reqString;
        // TODO: turn RPC's into strings for Network
        printf("[RaftServer.cc]: About to send RequestVote, term: %llu, serverId: %llu\n", currentTerm, config.serverId);
        for (auto& peer: config.clusterMap) {
            // NOTE: sendMessage only requires an addr, port, string
            network->sendMessage(peer.first, peer.second, reqString);
        }
    }

    void RaftServer::sendAppendEntriesRPCs(std::optional<bool> isHeartbeat = false) {
        Raft::RPC::AppendEntries::Request req;
        printf("[RaftServer.cc]: About to send AppendEntries, term: %llu, isHeartbeat\n", currentTerm, isHeartbeat.value());
        for (auto& peer: config.clusterMap) {
            // NOTE: sendMessage only requires an addr, port, string
            req.set_term(currentTerm);
            req.set_leaderid(config.serverId);
            req.set_prevlogindex(nextIndex[peer.first] - 1);
            uint64_t term;
            std::string entry;
            if(!storage->getLogEntry(nextIndex[peer.first] - 1, term, entry)) {
                std::cerr << "[RaftServer.cc]: Error while reading log for sendAppendEntries." << std::endl;
                exit(EXIT_FAILURE);
            }
            req.set_lastlogterm(term);

            // TODO: this needs to append multiple entries if need
            // Wondering if we do a local cache or always access memory
            if (isHeartbeat.value()) {
                req.set_entries({});
            } else {
                req.set_entries(entry);
            }

            // TODO: go over the most recent request_id stuff
            mostRecentRequestId[peer.first]++;
            req.set_requestid(mostRecentRequestId[peer.first]);

            std::string reqString;
            // TODO: turn RPC's into strings for Network
            network->sendMessage(peer.second.first, peer.second, reqString);
        }
    }

    void RaftServer::parseAndHandleNetworkMessage(std::string ipAddr, std::string networkMsg) {
        // TODO: Super pseudo code for now, need proto buf stuff etc

        // Option 1: Message is a Raft Server, or use a .find method idk
        for (auto& peer: config.clusterMap) {
            if (peer.second == addr) {
                Raft::RPC rpc;
                rpc = parseStringtoRPC(networkMsg); // TODO: figure out how we will parse strings with one of
                switch (rpc.type):
                    case Raft::RPC::AppendEntries::Request:
                        receivedAppendEntriesRPC(peer.first, rpc);
                    case Raft::RPC::AppendEntries::Response:
                        processAppendEntriesRPCResp(peer.first, rpc);
                    case Raft::RPC::RequestVote::Request:
                        receivedRequestVoteRPC(peer.first, rpc);
                    case Raft::RPC::RequestVote::Request:
                        processRequestVoteRPCResp(peer.first, rpc);  
                return;  
            }
        }

        // Option 2: Did not find a raftServer matching address, must be a RaftClient
        receivedClientCommandRPC(ipAddr, networkMsg);
    }

    void RaftServer::receivedClientCommandRPC(std::string ipAddr, Raft::RPC::ClientCommand cmd) {
        // Step 1: Append string cmd to log, get log index
        uint64_t nextLogIndex = storage->getLogLength() + 1;
        if (!storage->setLogEntry(nextLogIndex, currentTerm, networkMsg)) {
            std::cerr << "[RaftServer.cc]: Error while writing entry to log." << std::endl;
            exit(EXIT_FAILURE);
        }

        // Step 2: Associate log index with addr to respond to
        logToClientIPMap[nextLogIndex] = addr;

        // This should be it?, now the AppendEntriesRPC calls will try to propogate the whole log
        // Receipt of responses will let us know when indices are committed. 
    }

    void RaftServer::receivedAppendEntriesRPC(int peerId, Raft::RPC::AppendEntries::Request req) {
        printf("[RaftServer.cc]: Received Append Entries\n");
        Raft::RPC::AppendEntries::Response resp;
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
            resetTimer();

            // Update who the current leader is
            leaderId = peerId;

            /** Skipped all of the log replication
             * Dropping soon in Project 2 :P
             * Leaving some space here as a mental marker :)
            */
            resp.set_success(true);
        }
        // TODO: send the string not the RPC
        std::string respString = resp.ParseToString();
        network->sendMessage(config.clusterMap[peerId], respString);
    }

    void RaftServer::processAppendEntriesRPCResp(int peerId, Raft::RPC::AppendEntries::Response resp) {
        printf("[RaftServer.cc]: Process Append Entries Response\n");
        // If out of date, convert to follower before continuing
        if (resp.term() > currentTerm) {
            currentTerm = resp.term();
            votedFor = 0; // no vote casted in new term
            convertToFollower();
        }
        /* Do we do anything here for project 1? */
        // ignore if it is not the response to our msot recent request
        if (resp.requestid() != mostRecentRequestId[peerId]) {
            return;
        }

        /** Skipped all of the log replication
         * Dropping soon in Project 2 :P
         * Leaving some space here as a mental marker :)
        */
    }

    void RaftServer::receivedRequestVoteRPC(int peerId, Raft::RPC::RequestVote::Request req) {
        printf("[RaftServer.cc]: Received Request Vote\n");
        Raft::RPC::RequestVote::Response resp;

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
            resetTimer();
        } else {
            resp.set_votegranted(false);
        }
        // TODO: send the string not the RPC
        std::string respString = resp.ParseToString();
        network->sendMessage(config.clusterMap[peerId], respString);
    }

    void RaftServer::processRequestVoteRPCResp(int peerId, Raft::RPC::RequestVote::Response resp) {
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
            if (resp.votegranted() == true && myVotes.find(peerId) == myVotes.end()) {
                numVotesReceived += 1;
                myVotes.insert(peerId);
                if (numVotesReceived > (globals.config.clusterMap.size() / 2)) {
                    convertToLeader();
                }
            }
        }
    }
}