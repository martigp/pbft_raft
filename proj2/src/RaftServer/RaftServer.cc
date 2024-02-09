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
        , numVotesReceived ( 0 )
        , myVotes ( {} )
    {   
        config = ServerConfig(configPath, serverID);  
        try {  
            timer.reset(new Timer(&RaftServer::notifyRaftOfTimerEvent, *this));
            shellSM.reset(new ShellStateMachine(&RaftServer::notifyRaftOfStateMachineApplied, *this)); 
            network.reset(new NetworkManager(&RaftServer::notifyRaftOfNetworkMessage, *this)); 
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


            }
        }
    }

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
            // NOTE: sendMsg only requires an addr, port, string
            networkManager->sendMsg(peer.first, peer.second, reqString);
        }
    }

    void RaftServer::sendAppendEntriesRPCs(std::optional<bool> isHeartbeat = false) {
        Raft::RPC::AppendEntries::Request req;
        printf("[RaftServer.cc]: About to send AppendEntries, term: %llu, isHeartbeat\n", currentTerm, isHeartbeat.value());
        for (auto& peer: config.clusterMap) {
            // NOTE: sendMsg only requires an addr, port, string
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
            std::string reqString;
            // TODO: turn RPC's into strings for Network
            networkManager->sendMsg(peer.first, peer.second, reqString);
        }
    }
}