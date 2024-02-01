#include <string>
#include <cstdio>
#include <iostream>
#include "LogStateMachine.hh"
#include "Consensus.hh"

namespace Raft {
    
    LogStateMachine::LogStateMachine(Raft::Globals& globals)
        : globals(globals)
    {        
    }

    LogStateMachine::~LogStateMachine()
    {
    }

    void LogStateMachine::startUpdater(std::thread &stateMachineUpdaterThread) {
        stateMachineUpdaterThread = std::thread(&LogStateMachine::stateMachineLoop, this);
    }

    void LogStateMachine::stateMachineLoop() {
        while (true) {
            std::unique_lock<std::mutex> lock(stateMachineMutex);
            while (stateMachineQ.empty()) {
                stateMachineUpdatesCV.wait(lock);
            }

            while (!stateMachineQ.empty()) {
                std::string cmd = stateMachineQ.front();
                stateMachineQ.pop();
                std::string resp = proj1Execute(cmd);
                std::cout << cmd << std::endl;
                std::cout << resp << std::endl;
                std::cout << "access globals through SM: " << globals.config.serverID << std::endl;
            }

            for (int i = 0; i < 3; i++) {
                std::cout << "state machine restarting timer thread: " << i << std::endl;
                globals.consensus->resetTimer();
                std::this_thread::sleep_for (std::chrono::milliseconds(500));
            }
         }
    }

    std::string LogStateMachine::proj1Execute(std::string command) {
        // if (globals.consensus->myState != Consensus::ServerState::LEADER) {
        //     throw std::invalid_argument( "Cannot execute client command while not leader.");;
        // } else {
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
