#ifndef RAFT_RaftServer_H
#define RAFT_RaftServer_H

#include <string>
#include <random>
#include <memory>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <utility>
#include <libconfig.h++>
#include <unordered_map>
#include "RaftServer/ServerConfig.hh"
#include "RaftServer/ShellStateMachine.hh"
#include "RaftServer/Timer.hh"
#include "Protobuf/RaftRPC.pb.h"
#include "Common/RPC.hh"
#include "Common/NetworkService.hh"

namespace Raft {

    // TODO: Do we want to use this to make things easier to pass around 
    // string addr and int port before they make it to the network class?
    struct IPAddress {
        std::string addr;
        uint64_t port;
        bool operator==(const IPAddress& a) const
        {
        return (addr == a.addr && port == a.port);
        }
    };

    enum EventType {
        TIMER_FIRED,
        MESSAGE_RECEIVED,
        STATE_MACHINE_APPLIED
    };

    /**
     * RaftServerEvent is the generic wrapper for all incoming
     * events that will be queued up for the Raft Server to handle.
     * The EventQueue will be processed by a single thread, to ensure
     * no race conditions and provide a logic handling method
     * consistent with the Raft paper.
    */
    struct RaftServerEvent {
        EventType type;
        /* If type is MESSAGE_RECEIVED, this field will be set*/
        std::optional<std::string> ipAddr;
        std::optional<std::string> networkMsg;
        /* If type is STATE_MACHINE_APPLIED, these fields will be set*/
        std::optional<uint64_t> logIndex;
        std::optional<std::string> stateMachineResult;
    };

    class RaftServer {
        public:
            /**
             * @brief Construct a new RaftServer that stores the Global Raft State.
             * 
             * @param configPath The path of the configuration file. 
             * 
             * @param serverID Parsed from the command line, must be present in config file
             */
            RaftServer( std::string configPath, uint64_t serverID);

            /* Destructor */
            ~RaftServer();

            /**
             * @brief Start the RaftServer process
             */
            void start();

            /**
             * @brief Raft callback method for the Network
             * TODO: these are bad explanations
             * 
             * @param ipAddr IP Address from which a message was received
             * Formatted: "xx.xx.xx.xx:port"
             * 
             * @param networkMsg String contents of network message received
            */
            void notifyRaftOfNetworkMessage(std::string ipAddr, std::string networkMsg);

            /**
             * @brief Raft callback method for the Timer
            */
            void notifyRaftOfTimerEvent();

            /**
             * @brief Raft callback method for the State Machine
             * 
             * @param index Index of log entry that was applied
             * 
             * @param stateMachineResult Result of application of log entry 
             * to the State Machine
            */
            void notifyRaftOfStateMachineApplied(uint64_t logIndex, std::string stateMachineResult);

        
        private:
            /**************************************************************
             * Unique pointers to each of the other classes that support
             * the RaftServer functionality
            **************************************************************/
            std::unique_ptr<ShellStateMachine> shellSM;
            std::unique_ptr<Timer> timer;
            std::unique_ptr<Storage> storage;
            std::unique_ptr<Common::NetworkService> network;

            ServerConfig config;

            /**************************************************************
             * Below are the variables needed to manage the eventQueue:
             * the main interface through which timer and message received
             * events will be registered and handled by the RaftServer
            **************************************************************/

            /**
             * @brief Mutex for the eventQueue
            */
            std::mutex eventQueueMutex;

            /**
             * @brief Condition Variable used to notify of new events on 
             * the eventQueue
            */
            std::condition_variable eventQueueCV;

            /**
             * @brief Queue of events for the main RaftServer thread to handle
             * Each element is a struct of type RaftServerEvent
             * 
             * All external events(messages received, timer fired, state machine 
             * applied) will add their notification to this queue, so that the
             * single main thread for the RaftServer will respond in 
             * the order that the events were received.
            */
            std::queue<RaftServerEvent> eventQueue;

            /**************************************************************
             * Below are the variables/methods needed to manage consensus:
             * As specified by Figure 2
            **************************************************************/

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

            /*************************************
             * Persistent state on all servers
            **************************************/

            /**
             * @brief The latest term server has seen 
             * - initialized to 0 on first boot, increases monotonically
            */
            int32_t currentTerm;

            /**
             * @brief candidateID that received vote in current term 
             * - or 0 if none
            */
            int32_t votedFor;

            /**
             * @brief Index of highest log entry applied to state machine
             * - initialized to 0, increases monotonically
            */
            int32_t lastApplied;
            
            /*************************************
             * Volatile state on all servers
            **************************************/

            /**
             * @brief Index of highest log entry known to be committed
             * - initialized to 0, increases monotonically
            */
            int32_t commitIndex;

            /*************************************
             * Volatile state on all leaders
            **************************************/

            /**
             * @brief For each server, index of the next log entry to send to that server
             * - initialized to leader last log index +1
            */
            std::map<uint64_t, uint64_t> nextIndex;

            /**
             * @brief Index of highest log entry known to be replicated on server
             * - initialized to 0, increases monotonically
            */
            std::map<uint64_t, uint64_t> matchIndex;

            /*************************************
             * Below are all internal methods, etc
            **************************************/

            /**
             * @brief Most recent request_id for each RaftServer
             * Allows us to only process response to the most recent
             * request sent to each server
            */
            std::unordered_map<uint64_t, uint64_t> mostRecentRequestId;

            /**
             * @brief Method processes a callback received from the State Machine
             * prompting an update to lastApplied information.
             * Additionally, if the log index is associated with a Client IPAddr,
             * the RaftServer will send the response.
            */
            void handleAppliedLogEntry(uint64_t appliedIndex, std::string result);

            /**
             * @brief Given an IP Address and string message:
             *      Parse message and extract protobuf format
             *      Extract RaftServer ID or RaftClient based on IP
            */
            void parseAndHandleNetworkMessage(std::string ipAddr, std::string networkMsg);

            /**
             * @brief Receiver Implementation of AppendEntriesRPC
             * Sends back a response
             * Follows bottom left box in Figure 2
            */
            void receivedAppendEntriesRPC(int peerId, Raft::RPC::AppendEntries::Request req); 

            /**
             * @brief Sender Implementation of AppendEntriesRPC
             * Process the response received(term, success)
             * Follows bottom left box in Figure 2
            */
            void processAppendEntriesRPCResp(int peerId, Raft::RPC::AppendEntries::Response resp);

            /**
             * @brief Receiver Implementation of RequestVoteRPC
             * Sends back a response
             * Follows upper right box in Figure 2
            */
            void receivedRequestVoteRPC(int peerId, Raft::RPC::RequestVote::Request req); 

            /**
             * @brief Sender Implementation of RequestVoteRPC
             * Process the response received(term, voteGranted)
             * Follows upper right box in Figure 2
            */
            void processRequestVoteRPCResp(int peerId, Raft::RPC::RequestVote::Response resp);

            /**
             * @brief Mapping of log index to client IP addr
             * Once log indices are committed, respond to the 
             * corresponding client IP
            */
            std::map<uint64_t, uint64_t> logToClientIPMap;

            /**
             * @brief Receipt of new shell command from client
             * 
            */
            void receivedClientCommandRPC(std::string ipAddr, Raft::RPC::ClientCommand cmd);
            
            /**
             * @brief Decide action after timeout occurs
            */
            void timeoutHandler();

            /**
             * @brief Decide AppendEntries heartbeat time(hardcoded)
             * AND restart the timer with this new value
            */
            void setHeartbeatTimeout();

            /**
             * @brief Generate a new election interval 
             * AND restart the timer with the new value
            */
            void setRandomElectionTimeout();

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
             * @brief Format AppendEntries RPCs and send to other
             * servers in the cluster
             * 
             * @param isHeartbeat Optional bool to indicate that 
             * the entries field should be left empty
            */
            void sendAppendEntriesRPCs(std::optional<bool> isHeartbeat = false);

            /**
             * @brief After updating term, conversion to follow state
            */
            void convertToFollower();

            /**
             * @brief After winning election, convert to leader
            */
            void convertToLeader();

    }; // class RaftServer
} // namespace Raft

#endif /* RAFT_RAFTSERVER_H */