/**
 * Making a State Machine specific to the replicated log
 * TODO: If we have time, can make a generic StateMachine class and
 * do a wrapper for the replicated log specifics, such that Globals and Consensus
 * can have standardized calls to whatever StateMachine is present
*/

#ifndef RAFT_LOGSTATEMACHINE_H
#define RAFT_LOGSTATEMACHINE_H

#include <string>
#include <vector>

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
             * @brief Apply command at specific index
             * TODO: what needs to be passed, does the state machine hold the indices?
             * TODO: How does thread and returning an index look like
             *      What if the state machine ran a thread waiting to be notified of new commits, 
             *          thus this is always as up to date as possible (LogStateMachine.run())
             *      Then what if whatever function handles a command from client can also spawn a 
             *          thread that will wait until that index is applied (LogStateMachine.wait(7))
             *      Need locks and condition variables within this module since it will get notified
             *          by Consensus about new commits, needs to notify Consesus about application, then
             *          either Consensus or someone peering into here will tell the thread dealing with 
             *          RaftClient requests that it can return? I guess if multiple outstanding client 
             *          requests can exist, then we can spawn a thread for each that has the fd of the client
             *          and will wait until it's applied. thread can kill iteself after not being able to communicate
             *          back to client if that happens
             */
            apply(int logIdx, std::string command);

        private:
            /**
             * Log of applied commands
            */
            std::vector<std::string> commands;

    }; // class LogStateMachine
} // namespace Raft

#endif /* RAFT_LOGSTATEMACHINE_H */