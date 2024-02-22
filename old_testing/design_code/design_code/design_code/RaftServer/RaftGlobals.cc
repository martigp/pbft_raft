#include <sys/event.h>
#include "RaftGlobals.hh"
#include "Socket.hh"

namespace Raft {
    
    Globals::Globals(std::string configPath) 
        : timerThread()
        , serverListeningThread()
        , clientListeningThread()
        , stateMachineUpdaterThread()
    {     
        configPath = configPath;
        config = Common::ServerConfig(configPath); 

        raftServerThreads = std::vector<NamedThread>(config.numServers - 1);

        raftConsensus.reset(new Consensus(*this));
        serverSockets.reset(new ServerSocketManager(*this));
        clientSockets.reset(new ClientSocketManager(*this));
        stateMachine.reset(new LogStateMachine(*this));        
    }

    Globals::~Globals()
    {
    }

    void Globals::start() {
        raftConsensus->startTimer(timerThread);
        stateMachine->startUpdater(stateMachineUpdaterThread);

        serverSockets->start(serverListeningThread);
        clientSockets->start(clientListeningThread);
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
        for (int i = 0; i < config.numServers - 1; i++) {
            serverThreadMap[config.serverIds[i]] = clientSockets->sendRaftServerRPC(req);
        }
    }

}