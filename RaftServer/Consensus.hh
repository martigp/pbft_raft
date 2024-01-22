/**
 * Implementation of the Raft Consensus Protocol
 * 
 * Responsible for storing persistent state associated with Raft Consensus
 */

#ifndef RAFT_CONSENSUS_H
#define RAFT_CONSENSUS_H

#include <string>

namespace Raft {

    class Consensus {
        public:
            /**
             * @brief Constructor with the globals and config
            */
            explicit Consensus( Raft::Globals& globals, ServerConfig config, std::shared_ptr<Raft::LogStateMachine> stateMachine);

            /**
             * @brief Given an RPC request, process it
             * 
             * @return The response in RPC form
            */
            message processRPCRequests (message messages);

            /**
             * @brief Given a vector of RPC responses, process them
            */
            void processRPCResponses (std::vector<message> messages);

            /**
             * @brief Receiver Implementation of AppendEntriesRPC
             * Produces a response to send back
             * Follows bottom left box in Figure 2
            */
            AppendEntriesResponse processAppendEntriesRPC(AppendEntriesRequest req); 

            /**
             * @brief Sender Implementation of AppendEntriesRPC
             * Process the response received(term, success)
             * Follows bottom left box in Figure 2
            */
            void processAppendEntriesRPC(AppendEntriesRequest req);

            /**
             * @brief Receiver Implementation of RequestVoteRPC
             * Produces a response to send back
             * Follows upper right box in Figure 2
            */
            RequestVoteResponse processRequestVoteRPC(RequestVoteRequest req); 

            /**
             * @brief Sender Implementation of RequestVoteRPC
             * Process the response received(term, voteGranted)
             * Follows upper right box in Figure 2
            */
            void processRequestVoteRPC(RequestVoteRequest req); 

        private:
            /**
             * Enum for: Follower, Candidate, Leader
            */
            ServerState myState;

            /**
             * Private counter for number of votes received when running an election
            */
            int numVotesReceived;

            /**
             * Below are the figure 2 persistent state variables needed
             * Must be updated in stable storage before responding to RPCs
            */

            /**
             * The latest term server has seen 
             * - initialized to 0 on first boot, increases monotonically
            */
            int currentTerm;

            /**
             * candidateID that received vote in current term 
             * - or null if none
            */
            int votedFor;

            /**
             * Log entries, each entry contains command for state machine
             * and term when entry was received by leader (first index is 1)
            */
            std::vector<std::string> log;


            /**
             * Below is the volatile state on all servers
            */

            /**
             * Index of highest log entry known to be committed
             * - initialized to 0, increases monotonically
            */
            int commitIndex;

            /**
             * Index of highest log entry applied to state machine
             * - initialized to 0, increases monotonically
            */
            int lastApplied;


            /**
             * Below is the volatile state on all leaders
            */

            /**
             * For each server, index of the next log entry to send to that server
             * - initialized to leader last log index +1
            */
            std::vector<int> nextIndex;

            /**
             * Index of highest log entry known to be replicated on server
             * - initialized to 0, increases monotonically
            */
            std::vector<int> matchIndex[];
    }; // class Consensus
} // namespace Raft

#endif /* RAFT_CONSENSUS_H */
