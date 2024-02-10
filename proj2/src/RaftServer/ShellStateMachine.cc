#include <string>
#include <cstdio>
#include "ShellStateMachine.hh"
#include "Consensus.hh"

namespace Raft {
    
    ShellStateMachine::ShellStateMachine(Raft::Globals& globals)
        : globals(globals)
    {        
    }

    ShellStateMachine::~ShellStateMachine()
    {
    }

    void ShellStateMachine::startUpdater(std::thread &stateMachineUpdaterThread) {
        stateMachineUpdaterThread = std::thread(&ShellStateMachine::stateMachineLoop, this);
    }

    void ShellStateMachine::pushCmd(std::pair<uint64_t, std::string> cmd) {
        std::unique_lock<std::mutex> lock(stateMachineMutex);
        stateMachineQ.push(cmd);
    }

    void ShellStateMachine::stateMachineLoop() {
        while (true) {
            std::unique_lock<std::mutex> lock(stateMachineMutex);
            while (stateMachineQ.empty()) {
                stateMachineUpdatesCV.wait(lock);
            }

            while (!stateMachineQ.empty()) {
                std::pair<uint64_t, std::string> cmd = stateMachineQ.front();
                stateMachineQ.pop();

                printf("[Log State Machine]: Popped RaftClient command: %s", cmd.second.c_str());
                std::string ret = proj1Execute(cmd.second);

                Raft::RPC::StateMachineCmd::Response resp;
                resp.set_success(true);
                resp.set_leaderid(globals.config.serverId);
                resp.set_msg(ret);
                globals.serverSocketManager->sendRPC(cmd.first, resp, Raft::RPCType::STATE_MACHINE_CMD);
                printf("[Log State Machine]: sendRPC completed");
            }
        }
    }

    std::string ShellStateMachine::proj1Execute(std::string command) {
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
