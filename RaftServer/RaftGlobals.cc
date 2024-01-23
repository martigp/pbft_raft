#include <string>
#include <sys/event.h>
#include "RaftGlobals.hh"
#include "Socket.hh"

namespace Raft {
    
    Globals::Globals()
    {        
        raftConsensus = new Raft::Consensus();
        incomingSockets = new Raft::IncomingSocketManager();
        outgoingSockets = new Raft::OutgoingSocketManager();
        stateMachine = new Raft::LogStateMachine();         
    }

    Globals::~Globals()
    {
    }

    void Globals::init(std::string configPath) {
        this->configPath = configPath;
        // this->ServerConfig = Common::ServerConfig(this->configPath);
    }

}