#include <string>
#include <cstdio>
#include "ShellStateMachine.hh"

namespace Raft {
    
    // TODO: get correct values for commit index and last applied from
    // the server, may not be in constructor.
    ShellStateMachine::ShellStateMachine(RaftServer* server)
        : stateMachineUpdatesCV()
        , commandQueueMutex()
    {        
        callbackRaftServer = 
            std::bind(&RaftServer::notifyRaftOfStateMachineApplied,
                      server,
                      std::placeholders::_1, std::placeholders::_2);

        std::thread t(&ShellStateMachine::stateMachineLoop, this);
        t.detach();
    }

    ShellStateMachine::~ShellStateMachine()
    {
    }

    void ShellStateMachine::pushCmd(std::pair<uint64_t, std::string> cmd) {
        std::unique_lock<std::mutex> lock(commandQueueMutex);
        commandQueue.push(cmd);
        stateMachineUpdatesCV.notify_all();
    }

    void ShellStateMachine::stateMachineLoop() {
        std::unique_lock<std::mutex> lock(commandQueueMutex);
        while (true) {
            while (commandQueue.empty()) {
                stateMachineUpdatesCV.wait(lock);
            }

            std::pair<uint64_t, std::string> cmd = commandQueue.front();
            commandQueue.pop();
            lock.unlock();

            printf("[Log State Machine]: Popped RaftClient command: %s", cmd.second.c_str());

            try {
                std::string ret = applyCmd(cmd.second);
                printf("[Log State Machine]: Loge entry %llu applied", cmd.first);
                callbackRaftServer(cmd.first, ret);
            }
            catch(std::exception e) {
                std::cerr << "Failed to apply log entry " << cmd.first 
                          << std::endl;
            }

            lock.lock();
        }
    }

    std::string ShellStateMachine::applyCmd(const std::string& cmd) {
        std::string ret;
        const char *c = cmd.c_str();
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
