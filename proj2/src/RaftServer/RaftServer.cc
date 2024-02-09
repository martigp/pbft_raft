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
            serverSocketManager.reset(new ServerSocketManager(*this));
            consensus.reset(new Consensus(*this));
            logStateMachine.reset(new LogStateMachine(*this)); 
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
}