/**
 * Below is essentially all the inner workings and state of figure 2.
 * To simplify and minimize information leakage, this object operates
 * with limited information about the underlying network communication
 * implementation or the state machine. Like the Raft Paper, this class
 * can be applied to many applications, communication protocols, etc, so
 * long as messages can be sent and received, persistant state can be 
 * stored, and log entries can be applied to an underlying state machine.
 */

class RaftServer {
    public:
        /**
         * @brief Constructor with the server config and StateMachine
         * 
        */
        explicit RaftServer( ServerConfig config, StateMachine sm );

        /**
         * @brief Given an RPC request, process it
         * 
         * @return The response in RPC form
         * 
        */
        message processRPCRequests (message messages);

        /**
         * @brief Given a vector of RPC responses, process them
         * 
        */
        void processRPCResponses (std::vector<message> messages);

        /**
         * @brief Receiver Implementation of AppendEntriesRPC
         * Produces a response to send back
         * Follows bottom left box in Figure 2
         * 
        */
        AppendEntriesResponse processAppendEntriesRPC(AppendEntriesRequest req); 

        /**
         * @brief Sender Implementation of AppendEntriesRPC
         * Process the response received(term, success)
         * Follows bottom left box in Figure 2
         * 
        */
        void processAppendEntriesRPC(AppendEntriesRequest req);

        /**
         * @brief Receiver Implementation of RequestVoteRPC
         * Produces a response to send back
         * Follows upper right box in Figure 2
         * 
        */
        RequestVoteResponse processRequestVoteRPC(RequestVoteRequest req); 

        /**
         * @brief Sender Implementation of RequestVoteRPC
         * Process the response received(term, voteGranted)
         * Follows upper right box in Figure 2
         * 
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
         * currentTerm: The latest term server has seen 
         * - initialized to 0 on first boot, increases monotonically
        */
        int currentTerm;

        /**
         * votedFor: candidateID that received vote in current term 
         * - or null if none
        */
        int votedFor;

        /**
         * log[]: Log entries, each entry contains command for state machine
         * and term when entry was received by leader (first index is 1)
        */
        int log[];


        /**
         * Below is the volatile state on all servers
         *
        */

        /**
         * commitIndex: Index of highest log entry known to be committed
         * - initialized to 0, increases monotonically
        */
        int commitIndex;

        /**
         * lastApplied: Index of highest log entry applied to state machine
         * - initialized to 0, increases monotonically
        */
        int lastApplied;


        /**
         * Below is the volatile state on all leaders
         *
        */

        /**
         * nextIndex: For each server, index of the next log entry to send to that server
         * - initialized to leader last log index +1
        */
        int nextIndex[];

        /**
         * matchIndex: Index of highest log entry known to be replicated on server
         * - initialized to 0, increases monotonically
        */
        int matchIndex[];

}