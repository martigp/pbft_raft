#include <string>
#include <sys/event.h>
#include <libconfig.h++>
#include <iostream>
#include <cstdlib>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "RaftServer/RaftGlobals.hh"
// #include "RaftServer/Socket.hh"

namespace Raft {
    
    Globals::Globals( std::string configPath )
        : config(configPath),
          consensus(),
          logStateMachine(),
          nextUserEventId (FIRST_USER_EVENT_ID)
    {       
        consensus.reset(new Consensus(*this));
        logStateMachine.reset(new LogStateMachine(*this)); 
        persistentThreads = std::vector<std::thread>(2);
        // clientSocketManager.reset(new ClientSocketManager(*this));
        // serverSocketManager.reset(new ServerSocketManager(*this)); 
    }

    Globals::~Globals()
    {
    }

    void Globals::start()
    {
        /* Make the timer thread. */
        consensus->startTimer(persistentThreads[0]);
        /* Start the updater. */
        logStateMachine->startUpdater(persistentThreads[1]);

        std::cout << "started timer and state machine" << std::endl;
        
        /* Join persistent threads. All are in a while(true) */
        persistentThreads[0].join();
        persistentThreads[1].join();
    }

    uint32_t Globals::genUserEventId() {
        return nextUserEventId++;
    }
}