#include <string>
#include <sys/event.h>
#include "RaftGlobals.hh"
#include "Socket.hh"

namespace Raft {
    
    Globals::Globals()
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
    }

    std::string Globals::processRPCReq(std::string data, int serverID) {
        std::string ret;
        RaftRPC resp;

        RaftRPC rpc;
        rpc.ParseFromString(data);
        switch(rpc.payload) {
            case "logEntry":
                resp.payload = "logEntryResp";
                try {
                    resp.ret = stateMachine->proj1Execute(rpc);
                    resp.success = true;
                } catch (const std::invalid_argument& e) {
                    resp.ret = "";
                    resp.success = false; 
                }
            case "appendEntriesRequest":
                resp = raftConsensus.receivedRequestVoteRPC(rpc, serverID);
            case "appendEntriesResponse":
                break; // ERROR: should not get responses through ServerSocketManager
            case "requestVoteRequest":
                resp = raftConsensus.receivedRequestVoteRPC(rpc, serverID);
            case "requestVoteResponse":
                break; // ERROR: should not get responses through ServerSocketManager
            default:
                break; // maybe raise error here?
        }
        resp.SerializeToString(&ret);
        return ret;
    }

    void Globals::processRPCResp(std::string data, int serverID) {
        RaftRPC rpc;
        rpc.ParseFromString(data);
        switch(rpc.payload) {
            case "logEntry":
                break; // ERROR: should not get requests through ClientSocketManager
            case "appendEntriesRequest":
                break; // ERROR: should not get requests through ClientSocketManager
            case "appendEntriesResponse":
                processAppendEntriesRPCResp(rpc, serverID); 
            case "requestVoteRequest":
                break; // ERROR: should not get requests through ClientSocketManager
            case "requestVoteResponse":
                processRequestVoteRPCResp(rpc, serverID); 
            default:
                break; // maybe raise error here?
        }
    }

}