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

    void LogStateMachine::startUpdater(NamedThread &stateMachineUpdaterThread) {
        stateMachineUpdaterThread.myType = NamedThread::ThreadType::STATEMACHINEUPDATER;
        stateMachineUpdaterThread.thread = std::thread(stateMachineLoop, &stateMachineUpdaterThread);
        return;
    }

    // TODO: Need the USER FILT Stuff here!!!
    void LogStateMachine::stateMachineLoop(NamedThread *myThread) {
        while (!myThread->stop_requested) {
            std::unique_lock<std::mutex> lock(stateMachineMutex);
            while (stateMachineQ.empty()) {
                stateMachineUpdatesCV.wait(lock);
            }

            while (!stateMachineQ.empty()) {
                std::string resp = proj1Execute(stateMachineQ.pop());
            }
        }
    }

    std::string LogStateMachine::proj1Execute(RaftRPC rpc) {
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
