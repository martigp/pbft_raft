#include <libconfig.h++>
#include <iostream>
#include <cstdlib>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "RaftServer/RaftGlobals.hh"
#include "RaftServer/LogStateMachine.hh"
#include "RaftServer/Consensus.hh"
#include "RaftServer/Socket.hh"
#include <fstream>
#include <thread>
#include <filesystem>

namespace Raft {
    
    Globals::Globals( std::string configPath )
        : config( configPath ),
          networkService(*this),
          consensus(*this),
          logStateMachine(*this),
          mainThreads()          
    {
        sendMsgFn = std::bind(&Common::NetworkService::sendMessage,
                            &networkService,
                            std::placeholders::_1,
                            std::placeholders::_2);
    }

    Globals::~Globals()
    {
    }

    void Globals::start()
    {
        /* Start SSM listening. */
        // Get listen address
        std::thread networkThread(&Common::NetworkService::startListening,
                                  &networkService, config.listenAddr);

        mainThreads.push_back(networkThread);

        std::thread consensusThread(&Consensus::startTimer, &consensus);
        mainThreads.push_back(consensusThread);

        std::thread stateMachineThread(&LogStateMachine::stateMachineLoop, &logStateMachine);
        mainThreads.push_back(stateMachineThread);

        std::cout << "[RaftGlobals]: started SSM, CSM, timer and state machine" << std::endl;
        
        /* Join persistent threads. All are in a while(true) */
        for (auto& t : mainThreads) {
            t.join();
        }
    }
}