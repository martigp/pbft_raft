/**
 * Making a State Machine specific to the replicated log
 * TODO: If we have time, can make a generic StateMachine class and
 * do a wrapper for the replicated log specifics, such that Globals and Consensus
 * can have standardized calls to whatever StateMachine is present
 * 
 * TODO: Do project 2! Right now, this only executes a shell command. There is none
 * of the functionality required of the final replicated shell. Essentially only exists
 * to demonstrate abstraction that State Machine will exist outside of Consensus.
 * The method is called .proj1Execute() 
*/

#ifndef RAFT_LOGSTATEMACHINE_H
#define RAFT_LOGSTATEMACHINE_H

#include <string>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <queue>
#include "RaftGlobals.hh"

namespace Raft {

    class LogStateMachine {
        public:
            /**
             * @brief Construct a new LogStateMachine that applies the entries from Consensus
             * and also has reference back to globals
             */
            LogStateMachine(Raft::Globals& globals, std::shared_ptr<Raft::Consensus> consensus);

            /* Destructor */
            ~LogStateMachine();

            /**
             * @brief Begin state machine updater thread within StateMachine module
             * 
             * @return Thread ID for tracking in Global
            */
            void LogStateMachine::startUpdater(NamedThread &stateMachineUpdaterThread);

            /**
             * @brief Project 1 Specific: Execute command and return response.
             * For now, this will be called directly from the global handleClientRequest() method.
             * Returns the result 
             */
            std::string proj1Execute(RaftRPC rpc);

            /**
             * @brief StateMachine Updates CV 
             * Triggered by consensus committing new log entries
             * New entries to execute get added to the StateMachine queue
             * State Machine will not execute same log index twice
             * 
             * FOR NOW, in project 1, it will just pop off the queue, execute,
             * and notify our USER FILT in Server Socket Manager
            */
            std::condition_variable stateMachineUpdatesCV;

            /**
             * @brief Mutex for access to the StateMachine Queue
            */
            std::mutex stateMachineMutex;

            /**
             * @brief Queue of things to execute
             * TODO: what piece of info are client commands paired with to communicate back to
             * Server Socket Manager, string for now
            */
            std::queue<std::string> stateMachineQ;


        private:
            /**
             * @brief Reference to server globals
            */
            Raft::Globals& globals;

            /**
             * @brief Pointer to consensus module used for checking state
            */
            std::shared_ptr<Raft::Consensus> consensus;

            /**
             * @brief State Machine Loop
             * Currently has the stop mechanism included for demonstration purposes mostly
            */
            void stateMachineLoop(NamedThread *myThread);

    }; // class LogStateMachine
} // namespace Raft

#endif /* RAFT_LOGSTATEMACHINE_H */