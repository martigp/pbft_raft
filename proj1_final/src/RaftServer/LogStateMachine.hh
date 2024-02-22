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
#include "Protobuf/RaftRPC.pb.h"
#include "Common/RPC.hh"

namespace Raft {

    class Globals;

    class LogStateMachine {
        public:
            /**
             * @brief Construct a new LogStateMachine that applies the entries from Consensus
             * and also has reference back to globals
             */
            LogStateMachine(Raft::Globals& globals);

            /* Destructor */
            ~LogStateMachine();

            /**
             * @brief Begin state machine updater thread within StateMachine module
             * with thread passed in by reference
            */
            void startUpdater(std::thread &stateMachineUpdaterThread);

            /**
             * @brief Add (peerId, string) to the statemachine queue to be executed
            */
            void pushCmd(std::pair<uint64_t, std::string> cmd);

            /**
             * @brief Project 1 Specific: Execute command and return response.
             * For now, this will be called directly from the global handleClientRequest() method.
             * Returns the result 
             */
            std::string proj1Execute(std::string command);

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
             * Pair includes the clientId that issued the command and the 
             * string command to execute
            */
            std::queue<std::pair<uint64_t, std::string>> stateMachineQ;


        private:
            /**
             * @brief Reference to server globals
            */
            Raft::Globals& globals;

            /**
             * @brief State Machine Loop
             * Runs in  while(true) as one of the main threads in the program
            */
            void stateMachineLoop();

    }; // class LogStateMachine
} // namespace Raft

#endif /* RAFT_LOGSTATEMACHINE_H */