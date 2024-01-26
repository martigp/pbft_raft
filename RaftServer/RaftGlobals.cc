#include <string>
#include <sys/event.h>
#include "RaftGlobals.hh"
#include "Socket.hh"

namespace Raft {
    
    Globals::Globals() 
        : threadpool ( {} )
    {        
        raftConsensus = new Raft::Consensus();
        serverSockets = new Raft::ServerSocketManager();
        clientSockets = new Raft::ClientSocketManager();
        stateMachine = new Raft::LogStateMachine();         
    }

    Globals::~Globals()
    {
    }

    void Globals::init(std::string configPath) {
        this->configPath = configPath;
        this->config = Common::ServerConfig(this->configPath);
    }

    void Globals::start() {
        threadMap[NamedThread::ThreadType::TIMER] = raftConsensus->startTimer();
        threadMap[NamedThread::ThreadType::SERVERLISTENING] = serverSockets->start();
        threadMap[NamedThread::ThreadType::CLIENTLISTENING] = clientSockets->start();
    }

    RaftRPC Globals::processRPCReq(RaftRPC req, int serverID) {
        RaftRPC ret = raftConsensus->processRPCReq(req, serverID);
        return ret;
    }

    void Globals::processRPCResp(RaftRPC resp, int serverID) {
        raftConsensus->processRPCResp(resp, serverID);
    }

    void Globals::broadcastRPC(RaftRPC req) {
        // TODO: implement this, but this same string goes to all servers
        // TODO: will need a version that takes an array of strings and the servers that they go to
    }

}