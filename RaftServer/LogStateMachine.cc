#include <string>
#include <cstdio>
#include "LogStateMachine.hh"
#include "Consensus.hh"

namespace Raft {
    
    LogStateMachine::LogStateMachine(Raft::Globals& globals, std::shared_ptr<Raft::Consensus> consensus)
        : globals(globals)
        , consensus(consensus)
    {        
    }

    LogStateMachine::~LogStateMachine()
    {
    }

    std::string proj1Execute(RaftRPC rpc) {
        if (consensus->myState != Consensus::ServerState::LEADER) {
            throw std::invalid_argument( "Cannot execute client command while not leader.");;
        } else {
            std::string ret;
            std::string command = rpc.cmd();
            const char *c = command.c_str();
            FILE *pipe = popen(c, "r");
            if (pipe) { 
                char buffer[256]; 
                while (!feof(pipe)) { 
                    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) { 
                        ret.append(std::string(buffer));
                    } 
                } 
                pclose(pipe); 
            } 
            return ret;
        }
    }
}
