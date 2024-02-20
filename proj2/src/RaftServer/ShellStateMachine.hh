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
             * received from the server. Any variation of StateMachine can be 
             * created that matches the public interface of this ShellStateMachine.
             * 
             * @param callbackFn Method provided to the shell state machine to 
             * be invoked when a new entry has been applied. Allows the state machine
             * communicate the result of applying the entry at a specific index.
             * CallbackFn requires two arguments: uint64_t index 
             *                                    std::string result
             */
            ShellStateMachine(std::function<void(uint64_t, std::string *)> callbackFn);

            /* Destructor */
            ~ShellStateMachine();

            /**
             * @brief Method used by RaftServer to indicate to the State Machine that
             * commitIndex has been updated, along with new entries
             * 
             * Returns true on success, as the server is responsible for pushin
            */
            void pushCmd(uint64_t index, std::string cmd);

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
            std::string* applyCmd(const std::string& cmd);

            /**
             * @brief Function provided by RaftServer that will accept as arguments a
             * log index and a result once they have been applied
            */
            std::function<void(uint64_t, std::string*)> callbackRaftServer;

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
             * @brief Queue of outstanding indices and entries to commit
             * Min heap sorted by index. Index ensures we apply in order and only once.
             */
            struct StateMachineCommand {
                uint64_t index;
                std::string command;
            };

            struct CompareIndex {
                bool operator()(StateMachineCommand const& c1, StateMachineCommand const& c2)
                {
                    return c1.index > c2.index;
                }
            };

            std::priority_queue<StateMachineCommand, std::vector<StateMachineCommand>, CompareIndex> commandQueue;

            /**
             * @brief Store of last applied index to ensure entries are
             * applied in order and only once. Increases monotonically.
            */
            uint64_t lastApplied = 0;

    }; // class ShellStateMachine
} // namespace Raft

#endif /* RAFT_SHELLSTATEMACHINE_H */