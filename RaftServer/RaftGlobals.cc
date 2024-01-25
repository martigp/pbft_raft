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
        // this->ServerConfig = Common::ServerConfig(this->configPath);
        threadpool.push_back(raftConsensus->startTimer();)
    }

    std::string Globals::processRPCReq(std::string data, int serverID) {
        // has_appendentriesresponse()
        // set_has_appendentriesresponse()
        std::string ret;
        RaftRPC resp;

        RaftRPC rpc;
        rpc.ParseFromString(data);
        if (rpc.has_logentryrequest()) {
            LogEntryResponse respPayload;
            try {
                respPayload.set_ret(stateMachine->proj1Execute(rpc));
                respPayload.set_success(true);
            } catch (const std::invalid_argument& e) {
                respPayload.set_ret("");
                respPayload.set_success(false); 
            }
            resp.set_allocated_logentryresponse(&respPayload);
        } else if (rpc.has_appendentriesrequest()) {
            resp = raftConsensus->receivedRequestVoteRPC(rpc, serverID);
        } else if (rpc.has_requestvoterequest()) {
            resp = raftConsensus->receivedRequestVoteRPC(rpc, serverID);
        } else {
            return ""; // ERROR: received a request that we don't know how to handle, what do we shoot back?
        }
        resp.SerializeToString(&ret);
        return ret;
    }

    void Globals::processRPCResp(std::string data, int serverID) {
        // TODO: compile protobuf and see what actually shows up
        RaftRPC rpc;
        rpc.ParseFromString(data);
        if (rpc.has_appendentriesresponse()) {
            raftConsensus->processAppendEntriesRPCResp(rpc, serverID); 
        } else if (rpc.has_requestvoteresponse()) {
            raftConsensus->processRequestVoteRPCResp(rpc, serverID); 
        } else {
            return; // Is this an error? should only get responses to requests through ClientSocket Manager
        }
    }

    void Globals::broadcastRPC(RaftRPC req) {
        // TODO: implement this, but this same string goes to all servers
        // TODO: will need a version that takes an array of strings and the servers that they go to
    }

}