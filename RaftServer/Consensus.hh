/**
 * Implementation of the Raft Consensus Protocol
 * 
 * Responsible for storing persistent state associated with Raft Consensus
 */

#ifndef RAFT_CONSENSUS_H
#define RAFT_CONSENSUS_H

#include <string>
#include <random>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <thread>
#include "RaftGlobals.hh"
#include "LogStateMachine.hh"

namespace Raft {

    class Consensus {
        public:
            /**
             * @brief Constructor with the globals and config
            */
            explicit Consensus( Raft::Globals& globals, ServerConfig config, std::shared_ptr<Raft::LogStateMachine> stateMachine);

            /**
             * @brief Begin timer thread within consensus module
             * 
             * @return Thread ID for tracking in Global
            */
            Raft::NamedThread startTimer();

            /**
             * Enum for: Follower, Candidate, Leader as specified in Figure 2
            */
            enum class ServerState {
                FOLLOWER,
                CANDIDATE,
                LEADER
            };

            /**
             * @brief State of this server
            */
            ServerState myState;

            /**
             * @brief Receiver Implementation of AppendEntriesRPC
             * Produces a response to send back
             * Follows bottom left box in Figure 2
            */
            RaftRPC receivedAppendEntriesRPC(RaftRPC req, int serverID); 

            /**
             * @brief Sender Implementation of AppendEntriesRPC
             * Process the response received(term, success)
             * Follows bottom left box in Figure 2
            */
            void processAppendEntriesRPCResp(RaftRPC resp, int serverID);

            /**
             * @brief Receiver Implementation of RequestVoteRPC
             * Produces a response to send back
             * Follows upper right box in Figure 2
            */
            RaftRPC receivedRequestVoteRPC(RaftRPC req, int serverID); 

            /**
             * @brief Sender Implementation of RequestVoteRPC
             * Process the response received(term, voteGranted)
             * Follows upper right box in Figure 2
            */
            void processRequestVoteRPCResp(RaftRPC resp, int serverID); 

        private:
            /*************************************
             * References to global and other submodules
            **************************************/

            /**
             * @brief Reference to server globals
            */
            Raft::Globals& globals;

            /**
             * @brief The ServerConfig object.
             */
            Common::ServerConfig config;

            /**
             * @brief Pointer to state machine module (currently unused)
            */
            std::shared_ptr<Raft::LogStateMachine> stateMachine;

            /*************************************
             * Below are the figure 2 persistent state variables needed
             * Must be updated in stable storage before responding to RPCs
            **************************************/

            /**
             * @brief The latest term server has seen 
             * - initialized to 0 on first boot, increases monotonically
            */
            int currentTerm;

            /**
             * @brief candidateID that received vote in current term 
             * - or -1 if none
            */
            int votedFor;

            /**
             * @brief Log entries, each entry contains command for state machine
             * and term when entry was received by leader (first index is 1)
            */
            std::vector<std::string> log;


            /*************************************
             * Below is the volatile state on all servers
            **************************************/

            /**
             * @brief Index of highest log entry known to be committed
             * - initialized to 0, increases monotonically
            */
            int commitIndex;

            /**
             * @brief Index of highest log entry applied to state machine
             * - initialized to 0, increases monotonically
            */
            int lastApplied;


            /*************************************
             * Below is the volatile state on all leaders
            **************************************/

            /**
             * @brief For each server, index of the next log entry to send to that server
             * - initialized to leader last log index +1
            */
            std::vector<int> nextIndex;

            /**
             * @brief Index of highest log entry known to be replicated on server
             * - initialized to 0, increases monotonically
            */
            std::vector<int> matchIndex;

            /*************************************
             * Below are all internal methods, etc
            **************************************/

            /**
             * @brief Current election timeout length (milliseconds) 
             * Typically between 10-500ms
             * TODO: wb heart beat sending interval (paper says 0.5ms to 20 ms)
            */
            uint64_t timerTimeout;

            /**
             * @brief Generate a new election interval, assign to timerTimeout
            */
            void generateRandomElectionTimeout();

            /**
             * @brief Timer Loop
            */
            void timerLoop();

            /**
             * @brief Decide action after timeout occurs
            */
            void timeoutHandler();

            /**
             * @brief Assign appendEntries Heartbeat time to timerTimeout
            */
            void setHeartbeatTimeout();

            /**
             * @brief Method to not have duplicated code to reset the timer thread
            */
            void resetTimer();

            /**
             * @brief Timer reset CV 
             * Triggered by receiving AppendEntriesRPC(heartbeat) / ANY communication
            */
            std::condition_variable timerResetCV;

            /**
             * @brief Mutex access to bool heartbeatReceived
            */
            std::mutex resetTimerMutex;

            /**
             * @brief heartbeatReceived bool to determine if timer wakeup was spurious 
            */
            bool timerReset;

            /**
             * @brief Start a new Election:
             *      - increment currentTerm
             *      - vote for self
             *      - reset election timer
             *      - Send RequestVoteRPC to all servers
            */
            void startNewElection();

            /**
             * @brief Private counter for number of votes received when running an election
            */
            int numVotesReceived;

            /**
             * @brief After updating term, conversion to follow state(new election timeout and reset timer)
            */
            void convertToFollower();

            /**
             * @brief After winning election, convert to leader
            */
            void convertToLeader();

            /**
             * @brief As leader at heartbeat interval, send next AppendEntriesRPCs to the cluster
             * For now, this sends same empty thing with a broadcast to everyone
             * In Project 2, it will do log replication stuff
            */
            void sendAppendEntriesRPCs();
            
    }; // class Consensus
} // namespace Raft

#endif /* RAFT_CONSENSUS_H */
