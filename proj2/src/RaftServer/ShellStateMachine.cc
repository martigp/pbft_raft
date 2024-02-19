#include <string>
#include <cstdio>
#include "ShellStateMachine.hh"

namespace Raft {
    
    // TODO: get correct values for commit index and last applied from
    // the server, may not be in constructor.
    ShellStateMachine::ShellStateMachine(std::function<void(uint64_t, std::string* )> callbackFn)
        : callbackRaftServer(callbackFn)
        , stateMachineUpdatesCV()
        , commandQueueMutex()
    {        
        std::thread t(&ShellStateMachine::stateMachineLoop, this);
        t.detach();
    }

    ShellStateMachine::~ShellStateMachine()
    {
    }

    void ShellStateMachine::pushCmd(uint64_t index, std::string cmd) {
        std::unique_lock<std::mutex> lock(commandQueueMutex);
        commandQueue.push(StateMachineCommand{index, cmd});
        stateMachineUpdatesCV.notify_all();
    }

    void ShellStateMachine::stateMachineLoop() {
        std::unique_lock<std::mutex> lock(commandQueueMutex);
        while (true) {
            while (commandQueue.empty()) {
                stateMachineUpdatesCV.wait(lock);
            }

            StateMachineCommand smCmd = commandQueue.top();
            commandQueue.pop();
            lock.unlock();

            if (smCmd.index != lastApplied + 1) {
                printf("[Log State Machine]: Popped RaftClient command at index %llu, but last applied index is %llu\n", smCmd.index, lastApplied);
                // TODO: think about these edge cases more
            }

            printf("[Log State Machine]: Popped RaftClient command: %s", smCmd.command.c_str());

            try {
                std::string* ret = applyCmd(smCmd.command);
                printf("[Log State Machine]: Log entry %llu applied\n", smCmd.index);
                callbackRaftServer(smCmd.index, ret);
                lastApplied = smCmd.index;
            }
            catch(std::exception e) {
                std::cerr << "Failed to apply log entry " << smCmd.index
                          << std::endl;
            }

            lock.lock();
        }
    }

    std::string* ShellStateMachine::applyCmd(const std::string& cmd) {
        std::string *ret = new std::string();
        const char *c = cmd.c_str();
        FILE *pipe = popen(c, "r");
        if (pipe) { 
            char buffer[256]; 
            while (!feof(pipe)) { 
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) { 
                    ret->append(std::string(buffer));
                } 
            } 
            pclose(pipe); 
        } 
        return ret;
    }
}
