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
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <unordered_set>
#include "RaftGlobals.hh"
#include "LogStateMachine.hh"
#include "Protobuf/RaftRPC.pb.h"
#include "Common/RPC.hh"

namespace Raft {

    class Globals;

    class Consensus {
        public:
            /**
             * @brief Constructor with the globals and config
            */
            Consensus( Raft::Globals& globals);

            /* Destructor */
            ~Consensus();

            /**
             * @brief Begin timer thread within consensus module
             * 
             * @return Thread ID for tracking in Global
            */
            void startTimer(std::thread& timerThread);

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
             * @brief ServerId 
            */
            uint64_t serverId;

            /**
             * @brief Guess for current leader peerId 
            */
            uint64_t leaderId;

            /**
             * @brief Handle RaftClient Request received on our Server Socket Manager
             * 
             * @param peerId unique ID of RaftClient that RPC came from
             * 
             * @param header RPCHeader containing type and length for RPC decode
             * 
             * @param payload buffer payload of RPC
            */
            void handleRaftClientReq(uint64_t peerId, Raft::RPCHeader header, char *payload);

            /**
             * @brief Handle RaftServer Request received on our Server Socket Manager
             * 
             * @param peerId unique ID of RaftServer that RPC came from
             * 
             * @param header RPCHeader containing type and length for RPC decode
             * 
             * @param payload buffer payload of RPC
            */
            void handleRaftServerReq(uint64_t peerId, Raft::RPCHeader header, char *payload);

            /**
             * @brief Handle RaftServer Response received on our Client Socket Manager
             * 
             * @param peerId unique ID of RaftServer that RPC came from
             * 
             * @param header RPCHeader containing type and length for RPC decode
             * 
             * @param payload buffer payload of RPC
            */
            void handleRaftServerResp(uint64_t peerId, Raft::RPCHeader header, char *payload); 

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
            std::mutex persistentStateMutex;

            /**
             * @brief The latest term server has seen 
             * - initialized to 0 on first boot, increases monotonically
            */
            uint64_t currentTerm;

            /**
             * @brief candidateID that received vote in current term 
             * - or 0 if none
            */
            uint64_t votedFor;

            /**
             * @brief Log entries, each entry contains command for state machine
             * and term when entry was received by leader (first index is 1)
            */
            [[maybe_unused]] std::vector<std::string> log;

            /**
             * @brief Load in currentTerm and votedFor stored on disk(if any)
             * If no file is found, initializes currentTerm to 0 and votedFor to -1
            */
            void loadPersistentState();

            /**
             * @brief Write currentTerm and votedFor to disk
            */
            void writePersistentState();

            /*************************************
             * Below is the volatile state on all servers
            **************************************/

            /**
             * @brief Index of highest log entry known to be committed
             * - initialized to 0, increases monotonically
            */
            [[maybe_unused]] int commitIndex;

            /**
             * @brief Index of highest log entry applied to state machine
             * - initialized to 0, increases monotonically
            */
            [[maybe_unused]] int lastApplied;


            /*************************************
             * Below is the volatile state on all leaders
            **************************************/

            /**
             * @brief For each server, index of the next log entry to send to that server
             * - initialized to leader last log index +1
            */
            [[maybe_unused]] std::vector<int> nextIndex;

            /**
             * @brief Index of highest log entry known to be replicated on server
             * - initialized to 0, increases monotonically
            */
            [[maybe_unused]] std::vector<int> matchIndex;

            /*************************************
             * Below are all internal methods, etc
            **************************************/

            /**
             * @brief Receiver Implementation of AppendEntriesRPC
             * Sends back a response
             * Follows bottom left box in Figure 2
            */
            void receivedAppendEntriesRPC(Raft::RPC::AppendEntries::Request req, int peerId); 

            /**
             * @brief Sender Implementation of AppendEntriesRPC
             * Process the response received(term, success)
             * Follows bottom left box in Figure 2
            */
            void processAppendEntriesRPCResp(Raft::RPC::AppendEntries::Response resp, int peerId);

            /**
             * @brief Receiver Implementation of RequestVoteRPC
             * Sends back a response
             * Follows upper right box in Figure 2
            */
            void receivedRequestVoteRPC(Raft::RPC::RequestVote::Request req, int peerId); 

            /**
             * @brief Sender Implementation of RequestVoteRPC
             * Process the response received(term, voteGranted)
             * Follows upper right box in Figure 2
            */
            void processRequestVoteRPCResp(Raft::RPC::RequestVote::Response resp, int peerId);

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
             * @brief Set for which servers have voted for you in current election
             * Avoids double counting votes from same server
            */
            std::unordered_set<int> myVotes;

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

    class PersistentState {
        public:
            uint64_t term;
            uint64_t votedFor;
    }; // class PersistentState

} // namespace Raft

#endif /* RAFT_CONSENSUS_H */
