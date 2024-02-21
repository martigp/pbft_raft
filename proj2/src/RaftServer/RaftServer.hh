#ifndef RAFT_RAFTSERVER_H
#define RAFT_RAFTSERVER_H

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
#include "Common/RaftConfig.hh"
#include "RaftServer/ShellStateMachine.hh"
#include "RaftServer/Timer.hh"
#include "RaftServer/ServerStorage.hh"
#include "Protobuf/RaftRPC.pb.h"
#include "Common/NetworkService.hh"

namespace Raft {

    class ShellStateMachine;
    class Timer;
    class ServerStorage;

    /**
     * RaftServerEvent is the generic wrapper for all incoming
     * events that will be queued up for the Raft Server to handle.
     * The EventQueue will be processed by a single thread, to ensure
     * no race conditions and provide a logic handling method
     * consistent with the Raft paper.
    */
    struct RaftServerEvent {
        enum {TIMER_FIRED,
              MESSAGE_RECEIVED,
              STATE_MACHINE_APPLIED} type;
        /* If type is MESSAGE_RECEIVED, this field will be set*/
        std::optional<std::string> addr = std::nullopt;
        std::optional<std::string> msg = std::nullopt;
        /* If type is STATE_MACHINE_APPLIED, these fields will be set*/
        std::optional<uint64_t> logIndex = std::nullopt;
        std::optional<std::string *> stateMachineResult = std::nullopt;
    };

    class RaftServer : public Common::NetworkUser {
        public:
            /**
             * @brief Construct a new RaftServer that stores the Global Raft State.
             * 
             * @param configPath The path of the configuration file. 
             * 
             * @param firstServerBoot Whether this is the first time that the
             * server has run.
             */
            RaftServer(const std::string& configPath, bool firstServerBoot);

            /* Destructor */
            ~RaftServer();

            /**
             * @brief Start the RaftServer process
             */
            void start();

            /**
             * @brief Overriden function that is called by the Network Service
             * that the RaftServer is a user of when it has received a message.
             * 
             * @param sendAddr Host address from which a message was received
             * Formatted: "xx.xx.xx.xx:port"
             * 
             * @param networkMsg Network message received
            */
            void handleNetworkMessage(const std::string& sendAddr,
                                      const std::string& networkMsg);

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
            void notifyRaftOfStateMachineApplied(uint64_t logIndex,
                                                 std::string* stateMachineResult);

        
        private:
            /**************************************************************
             * Other classes that support RaftServer functionality
            **************************************************************/
            /**
             * @brief The configuration class that stores all of a Raft Server's
             * configuration information. 
             */
            Common::RaftConfig config;

            /**
             * @brief Handles all persistent state as specified by Raft Figure 2.
             * 
             * Includes:
             *  - currentTerm
             *  - votedFor
             *  - lastApplied
             *  - log
             */
            Raft::ServerStorage storage;

            /**
             * @brief State Machine that executes commands that have been committed,
             * TODO: FIX THIS TO ACCESS PERSISTENT STATE 
             */
            std::unique_ptr<Raft::ShellStateMachine> shellSM;

            /**
             * @brief Timer module with generic callback function to Raft Server,
             * that allows resetting of the timer and it's timeout period.
            */
            std::unique_ptr<Raft::Timer> timer;

            /**
             * @brief The service used by the Raft Server to send and receive
             * messages on the network.
             */
            Common::NetworkService network;

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
            ServerState raftServerState;

            
            /*************************************
             * Volatile state on all servers
            **************************************/

            /**
             * @brief Index of highest log entry known to be committed
             * - initialized to 0, increases monotonically
            */
            uint64_t commitIndex;

            /**
             * @brief The Raft Server Id of the current leader.
             * 
             */
            uint64_t leaderId;

            /*************************************
             * Volatile state about other raft servers needed by the leader.
            **************************************/
           
            struct RaftServerVolatileState {
                /**
                 * @brief The index of the next log entry to send to the server.
                 * Initialized to leader last log index + 1
                 * 
                 */
                uint64_t nextIndex;
                /**
                 * @brief Index of the highest known log entry to be replicated
                 * on the server. Initialized to 0, increases monotonically
                 * 
                 */
                uint64_t matchIndex;
                /**
                 * @brief As name. Ensures only the server's response to the
                 * most recent request sent to it is processed.
                 * 
                 */
                uint64_t mostRecentRequestId;
            };
           
            /**
            * @brief Centralized record of all per server volatile state
            * 
            */
            std::unordered_map<uint64_t, struct RaftServerVolatileState>
                    volatileServerInfo;

            /**
             * @brief Method processes a callback received from the State Machine
             * prompting an update to lastApplied information.
             * Additionally, if the log index is associated with a Client IPAddr,
             * the RaftServer will send the response.
            */
            void handleAppliedLogEntry(uint64_t appliedIndex,
                                       std::string* result);

            /**
             * @brief Given an IP Address and string message:
             *      Parse message and extract protobuf format
             *      Extract RaftServer ID or RaftClient based on IP
            */
            void processNetworkMessage(const std::string& senderAddr,
                                              const std::string& networkMsg);

            /**
             * @brief Receiver Implementation of AppendEntriesRPC
             * Sends back a response
             * Follows bottom left box in Figure 2
            */
            void processAppendEntriesReq(const std::string& senderAddr, 
                                         const RPC_AppendEntries_Request& req); 

            /**
             * @brief Sender Implementation of AppendEntriesRPC
             * Process the response received(term, success)
             * Follows bottom left box in Figure 2
            */
            void processAppendEntriesResp(const std::string& senderAddr,
                                          const RPC_AppendEntries_Response& resp);

            /**
             * @brief Receiver Implementation of RequestVoteRPC
             * Sends back a response
             * Follows upper right box in Figure 2
            */
            void processRequestVoteReq(const std::string& senderAddr,
                                       const RPC_RequestVote_Request& req); 

            /**
             * @brief Sender Implementation of RequestVoteRPC
             * Process the response received(term, voteGranted)
             * Follows upper right box in Figure 2
            */
            void processRequestVoteResp(const std::string& senderAddr,
                                        const RPC_RequestVote_Response& resp);

            /**
             * @brief Mapping of log index to information about the corresponding
             * request - requestId and requesting address ip:port
             */
            std::map<uint64_t, 
                     std::pair<uint64_t, std::string>> logToClientRequestMap;

            /**
             * @brief Receipt of new shell command from client
             * 
            */
            void processClientRequest(const std::string& clientAddr,
                                      const RPC_StateMachineCmd_Request& cmd);
            
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
             * @brief Format and attempts to send AppendEntries Requests to a single server. 
             * Fails silently.
            */
            void sendAppendEntriesReq(uint64_t serverId, std::string serverAddr);

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