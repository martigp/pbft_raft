#include <sys/event.h>
#include <libconfig.h++>
#include <iostream>
#include <cstdlib>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "RaftServer/RaftGlobals.hh"
#include "RaftServer/Socket.hh"

namespace Raft {
    
    Globals::Globals( std::string configPath )
        : config( configPath ),
          clientSocketManager(),
          serverSocketManager(),
          consensus(),
          logStateMachine(),
          nextUserEventId (FIRST_USER_EVENT_ID)
    {       
        clientSocketManager.reset(new ClientSocketManager(*this));
        serverSocketManager.reset(new ServerSocketManager(*this));
        consensus.reset(new Consensus(*this));
        logStateMachine.reset(new LogStateMachine(*this)); 
        mainThreads = std::vector<std::thread>(4); 
    }

    Globals::~Globals()
    {
    }

    void Globals::start()
    {
        /* Start SSM listening. */
        serverSocketManager->start(mainThreads[0]);

        /* Start SSM listening. */
        serverSocketManager->start(mainThreads[1]);

        /* Start the timer thread. */
        consensus->startTimer(mainThreads[2]);

        /* Start the updater. */
        logStateMachine->startUpdater(mainThreads[3]);

        std::cout << "LOG: started SSM, CSM, timer and state machine" << std::endl;
        
        /* Join persistent threads. All are in a while(true) */
        mainThreads[0].join();
        mainThreads[1].join();
        mainThreads[2].join();
        mainThreads[3].join();
    }

    uint32_t Globals::genUserEventId() {
        return nextUserEventId++;
    }
}