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

    std::string proj1Execute(std::string command) {
        if (consensus->myState != Consensus::ServerState::LEADER) {
            throw consensus->myState;
        } else {
            std::string ret;
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
