#ifndef RAFT_SHELLSTATEMACHINE_H
#define RAFT_SHELLSTATEMACHINE_H

#include <string>
#include <vector>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <functional>
#include "RaftServer/RaftServer.hh"
#include "Protobuf/RaftRPC.pb.h"

namespace Raft {

    class RaftServer;

    class ShellStateMachine {
        public:
            /**
             * @brief Construct a new ShellStateMachine that applies log entries
             * 
             * @param callbackFn The raft server that the shell state machine is
             * plugged into.
             * 
             * TODO: make sure this gets lastApplied on reboot
             */
            ShellStateMachine(std::function<void(uint64_t, const std::string)> callbackFn);

            /* Destructor */
            ~ShellStateMachine();

            /**
             * @brief Method used by RaftServer to indicate to the State Machine that
             * commitIndex has been updated
             * 
             * Does not return success or failure, as RaftServer will eventually 
             * indicate future, monotonically increasing commit indices
            */
            void newCommitIndex(uint64_t commitIndex);


            void pushCmd(std::pair<uint64_t, std::string> cmd);


        private:
            /**
             * @brief State Machine Loop
             * Flow of events:
             * Notified by RaftServer of a new committed index
             * Access persistent state to retrieve log entry
             *      Confirm that the index has not yet been applied(stored in persistent)
             * Apply the entry
             * Use the callback function to return a result 
             *      NOTE: so that there is only one writer to a file, RaftServer will
             *      write to persistent state to update lastApplied after StateMahcine calls
             *      back to it 
            */
            void stateMachineLoop();

            /**
             * @brief Executes the provided command locally.
             * 
             * @param command Command to executed
             * @return std::string The shell response from running the command
             */
            std::string applyCmd(const std::string& cmd);

            /**
             * @brief Function provided by RaftServer that will accept as arguments a
             * log index and a result once they have been applied
            */
            std::function<void(uint64_t, const std::string)> callbackRaftServer;

            /**
             * @brief StateMachine Updates CV 
             * Triggered by RaftServer committing new log entries
             * New highest commitIndex has been stored
            */
            std::condition_variable stateMachineUpdatesCV;

            /**
             * @brief Mutex to synchronize access to the commandQueue
            */
            std::mutex commandQueueMutex;

            /**
             * @brief 
             * 
             */
            std::queue<std::pair<uint64_t, std::string>> commandQueue;

    }; // class ShellStateMachine
} // namespace Raft

#endif /* RAFT_SHELLSTATEMACHINE_H */