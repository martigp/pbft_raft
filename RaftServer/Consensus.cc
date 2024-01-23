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

}
