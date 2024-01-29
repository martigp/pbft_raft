#include <string>
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
          nextUserEventId (FIRST_USER_EVENT_ID)
    {   
        try {
        clientSocketManager.reset(new ClientSocketManager(*this));
        serverSocketManager.reset(new ServerSocketManager(*this)); 
        }
        catch(const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    Globals::~Globals()
    {
    }

    void Globals::start()
    {
        /* Make the timer thread. */

        /* Start listening. */
        serverSocketManager->start();
        clientSocketManager->start();
    }

    uint32_t Globals::genUserEventId() {
        return nextUserEventId++;
    }
}