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
          serverSocketManager()
    {       
        clientSocketManager.reset(new ClientSocketManager(*this));
        serverSocketManager.reset(new ServerSocketManager(*this)); 
    }

    Globals::~Globals()
    {
    }

    void Globals::start()
    {
        /* Make the timer thread. */

        /* Start listening. */
        serverSocketManager->start();
    }
}