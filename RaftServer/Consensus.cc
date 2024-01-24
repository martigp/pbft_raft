#include <string>
#include "Consensus.hh"

namespace Raft {
    
    Consensus::Consensus( Raft::Globals& globals, ServerConfig config, std::shared_ptr<Raft::LogStateMachine> stateMachine)
        : globals(globals)
        , config(config)
        , stateMachine(stateMachine)
    {        
    }

    Consensus::~Consensus()
    {
    }

    RaftRPC Consensus::receivedAppendEntriesRPC(RaftRPC req, int serverID); {

    }

    void Consensus::processAppendEntriesRPCResp(RaftRPC resp, int serverID) {

    }

    RaftRPC Consensus::receivedRequestVoteRPC(RaftRPC req, int serverID) {

    }

    void Consensus::processRequestVoteRPCResp(RaftRPC resp, int serverID) {

    }

}
