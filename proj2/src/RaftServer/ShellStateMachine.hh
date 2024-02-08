#ifndef RAFT_SHELLSTATEMACHINE_H
#define RAFT_SHELLSTATEMACHINE_H

#include <string>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <functional>
#include "RaftGlobals.hh"
#include "Protobuf/RaftRPC.pb.h"
#include "Common/RPC.hh"

namespace Raft {

    class Globals;

    class ShellStateMachine {
        public:
            /**
             * @brief Construct a new ShellStateMachine that applies log entries
             * 
             * @param callback Function provided by RaftServer to callback once
             * state machine entres have been applied
             * 
             * TODO: make sure this gets lastApplied on reboot
             */
            ShellStateMachine(std::function<void<int32_t, std::string>> callback);

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
             * @brief Function provided by RaftServer that will accept as arguments a
             * log index and a result once they have been applied
            */
            std::function<void<int32_t, std::string>> callbackRaftServer;

            /**
             * @brief StateMachine Updates CV 
             * Triggered by RaftServer committing new log entries
             * New highest commitIndex has been stored
            */
            std::condition_variable stateMachineUpdatesCV;

            /**
             * @brief Mutex for access to the commitIndex
            */
            std::mutex stateMachineMutex;

            /**
             * @brief  Index of highest log entry known to be committed,
             * as notified of by RaftServer
            */
            uint64_t commitIndex;

            /**
             * @brief  Index of highest log entry known to be applied,
             * will be sent back to RaftServer in callback as each entry
             * is applied
            */
            uint64_t lastApplied;

    }; // class ShellStateMachine
} // namespace Raft

#endif /* RAFT_SHELLSTATEMACHINE_H */