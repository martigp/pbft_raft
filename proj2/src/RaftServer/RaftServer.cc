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
    
    RaftServer::RaftServer( std::string configPath )
        : config( configPath )
        , network( )
        , timer( )
        , storage( )
        , myState ( Consensus::ServerState::FOLLOWER )
        , serverId ( globals.config.serverId )
        , leaderId ( 0)
        , currentTerm ( 0 )
        , votedFor ( 0 )
        , log ( {} )
        , commitIndex ( 0 )
        , lastApplied ( 0 )
        , nextIndex ( 0 )
        , matchIndex ( {} )
        , mostRecentRequestId ( 0 )
        , timerTimeout ( 0 )
        , timerReset ( false )
        , numVotesReceived ( 0 )
        , myVotes ( {} )
    {     
        try {  
            clientSocketManager.reset(new ClientSocketManager(*this));
            serverSocketManager.reset(new ServerSocketManager(*this));
            consensus.reset(new Consensus(*this));
            logStateMachine.reset(new LogStateMachine(*this)); 
            mainThreads = std::vector<std::thread>(4);
        } catch(const std::exception& e) {
            std::cerr << e.what() << std::endl;
        } 
    }

    RaftServer::~RaftServer()
    {
    }

    void RaftServer::start()
    {
        /* Start SSM listening. */
        serverSocketManager->startListening(mainThreads[0]);

        /* Start CSM listening. */
        clientSocketManager->startListening(mainThreads[1]);

        /* Start the timer thread. */
        consensus->startTimer(mainThreads[2]);

        /* Start the updater. */
        logStateMachine->startUpdater(mainThreads[3]);

        std::cout << "[RaftRaftServer]: started SSM, CSM, timer and state machine" << std::endl;
        
        /* Join persistent threads. All are in a while(true) */
        mainThreads[0].join();
        mainThreads[1].join();
        mainThreads[2].join();
        mainThreads[3].join();
    }
}