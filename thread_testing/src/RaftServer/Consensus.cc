#include "Consensus.hh"

namespace Raft {
    
    Consensus::Consensus( Raft::Globals& globals)
        : myState( Consensus::ServerState::FOLLOWER )
        , globals(globals)
        // , numVotesReceived ( 0 )
        , timerTimeout ( 0 )
        // , currentTerm ( 0 )
        // , votedFor ( -1 )
        // , log ( {} )
        // , commitIndex ( 0 )
        // , lastApplied ( 0 )
        // , nextIndex ( 0 )
        // , matchIndex ( {} )
    {        
    }

    // Consensus::~Consensus()
    // {
    // }

    void Consensus::startTimer(std::thread &timerThread) {
        generateRandomElectionTimeout();
        timerThread = std::thread(&Consensus::timerLoop, this);
    }

    // void Consensus::processRPCResp(RaftRPC resp, int serverID) {
    //     if (resp.has_appendentriesresponse()) {
    //         processAppendEntriesRPCResp(resp, serverID); 
    //     } else if (resp.has_requestvoteresponse()) {
    //         processRequestVoteRPCResp(resp, serverID); 
    //     } else {
    //         return; // Is this an error? should only get responses to requests through ClientSocket Manager
    //     }
    // }

    // RaftRPC Consensus::processRPCReq(RaftRPC req, int serverID) {
    //     RaftRPC resp;
    //     if (req.has_logentryrequest()) {
    //         LogEntryResponse respPayload; // TODO: this is ok for now, but in the future needs to spawn thread and return
    //         try {
    //             respPayload.set_ret(stateMachine->proj1Execute(req));
    //             respPayload.set_success(true);
    //         } catch (const std::invalid_argument& e) {
    //             respPayload.set_ret("");
    //             respPayload.set_success(false); 
    //         }
    //         resp.set_allocated_logentryresponse(&respPayload);
    //     } else if (req.has_appendentriesrequest()) {
    //         resp = receivedAppendEntriesRPC(req, serverID);
    //     } else if (req.has_requestvoterequest()) {
    //         resp = receivedRequestVoteRPC(req, serverID);
    //     } else {
    //         return ""; // ERROR: received a request that we don't know how to handle, what do we shoot back?
    //     }
    // }

    void Consensus::timerLoop() {
        while (true) {
            std::unique_lock<std::mutex> lock(resetTimerMutex);
            if (timerResetCV.wait_for(lock, std::chrono::milliseconds(timerTimeout), [&]{ return timerReset == true; })) {
                timerReset = false;
                std::cout << "Timer Reset" << std::endl;
                continue; // TODO: add log, I think this will just restart the loop
            } else {
                std::cout << "Timed Out" << std::endl;
                timeoutHandler();
            }
        }
    }

    void Consensus::timeoutHandler() {
        communicateWithSMTest();
        // switch (myState) {
        //     case ServerState::FOLLOWER:
        //         communicateWithSMTest();
        //         // myState = ServerState::CANDIDATE;
        //         // startNewElection();
        //     case ServerState::CANDIDATE:
        //         // startNewElection();
        //     case ServerState::LEADER:
        //         // sendAppendEntriesRPCs();
        // }
    }

    void Consensus::communicateWithSMTest() {
        std::cout << "About to communicate with State Machine" << std::endl;
        std::unique_lock<std::mutex> lock(globals.logStateMachine->stateMachineMutex);
        globals.logStateMachine->stateMachineQ.push("pwd\n");
        globals.logStateMachine->stateMachineUpdatesCV.notify_all();
        std::cout << "Notified" << std::endl;
    }

    void Consensus::resetTimer() {
        resetTimerMutex.lock();
        timerReset = true;
        timerResetCV.notify_all();
        resetTimerMutex.unlock();
    }

    void Consensus::generateRandomElectionTimeout() {
        std::unique_lock<std::mutex> lock(resetTimerMutex);
        std::random_device seed;
        std::mt19937 gen{seed()}; 
        std::uniform_int_distribution<> dist{5000, 10000};
        timerTimeout = dist(gen);
    }

    void Consensus::setHeartbeatTimeout() {
        std::unique_lock<std::mutex> lock(resetTimerMutex);
        timerTimeout = 1000; // TODO: is this right?
    }
}

    // void Consensus::startNewElection() {
    //     std::unique_lock<std::mutex> lock(persistentStateMutex);
    //     currentTerm += 1;
    //     votedFor = globals.config.serverID;
    //     // TODO: WRITE TO DISK
    //     myVotes.clear();
    //     numVotesReceived = 1;
    //     resetTimer();

        // RaftRPC req;
        // RequestVoteRequest payload;
        // payload.set_term(currentTerm);
        // payload.set_candidateid(globals.config.myServerID);
        // payload.set_lastlogindex(0);
        // payload.set_lastlogterm(0);
        // req.set_allocated_requestvoterequest(&payload);
        // std::string reqString;
        // req.SerializeToString(&reqString);
        // globals.broadcastRPC(reqString);
    // }

    // void Consensus::convertToFollower() {
    //     myState = ServerState::FOLLOWER;
    //     generateRandomElectionTimeout();
    //     resetTimer();    
    // }

    // void Consensus::convertToLeader() {
    //     myState = ServerState::LEADER;
    //     setHeartbeatTimeout();
    //     resetTimer(); // so not interrupted again 

        // format initial empty AppendEntriesRPC heartbeat
        // RaftRPC req;
        // AppendEntriesRequest payload;
        // payload.set_term(currentTerm);
        // payload.set_leaderid(config.myServerID);
        // payload.set_prevlogindex(0); // will change for proj 2
        // payload.set_prevlogterm(0); // will change for proj 2
        // payload.add_entries();
        // payload.set_leadercommit(0); // will change for proj 2
        // req.set_allocated_appendentriesrequest(&payload);
        // std::string reqString;
        // req.SerializeToString(&reqString);
        // globals.broadcastRPC(reqString);
    // }

    // void Consensus::sendAppendEntriesRPCs() {
        // format AppendEntriesRPCs for each RaftServer
        // right now always empty heartbeats
        // RaftRPC req;
        // AppendEntriesRequest payload;
        // payload.set_term(currentTerm);
        // payload.set_leaderid(config.myServerID);
        // payload.set_prevlogindex(0); // will change for proj 2
        // payload.set_prevlogterm(0); // will change for proj 2
        // payload.add_entries();
        // // LogEntry* le = payload.add_entries();
        // // le->set_cmd(cmd);
        // // le->set_term(term);
        // payload.set_leadercommit(0); // will change for proj 2
        // req.set_allocated_appendentriesrequest(&payload);
        // std::string reqString;
        // req.SerializeToString(&reqString);
        // globals.broadcastRPC(reqString);
    // }

    // RaftRPC Consensus::receivedAppendEntriesRPC(RaftRPC req, int serverID) {
    //     RaftRPC resp;
    //     AppendEntriesResponse payload; 
    //     std::unique_lock<std::mutex> lock(persistentStateMutex);

    //     // If out of date, convert to follower before continuing
    //     if (req.appendentriesrequest().term() > currentTerm) {
    //         currentTerm = resp.appendentriesrequest().term();
    //         votedFor = -1; // no vote casted in new term
    //         convertToFollower();
    //     }

    //     // Currently running an election, convert to follower and continue
    //     if (myState == ServerState::CANDIDATE && req.appendentriesrequest().term() == currentTerm) {
    //         convertToFollower(); 
    //     }

    //     // Include requestID in response for RPC pairing
    //     payload.set_term(currentTerm);
    //     payload.set_requestid(req.appendentriesrequest().requestid());
        
    //     if (req.appendentriesrequest().term() < currentTerm) {
    //         payload.set_success(false);
    //         resp.set_allocated_appendentriesresponse(&payload);
    //         return resp;
    //     } else {
    //         // At this point, we must be talking to the currentLeader, resetTimer as specified in Rules for Followe
    //         resetTimer();

    //         /** Skipped all of the log replication
    //          * Dropping soon in Project 2 :P
    //          * Leaving some space here as a mental marker :)
    //         */
    //         payload.set_success(true);
    //         resp.set_allocated_appendentriesresponse(&payload);
    //         return resp;
    //     }
    // }

    // void Consensus::processAppendEntriesRPCResp(RaftRPC resp, int serverID) {
    //     std::unique_lock<std::mutex> lock(persistentStateMutex);
    //     // If out of date, convert to follower before continuing
    //     if (resp.appendentriesresponse().term() > currentTerm) {
    //         currentTerm = resp.appendentriesresponse().term();
    //         votedFor = -1; // no vote casted in new term
    //         convertToFollower();
    //     }
    //     /* Do we do anything here for project 1? */

    //     /** Skipped all of the log replication
    //      * Dropping soon in Project 2 :P
    //      * Leaving some space here as a mental marker :)
    //     */
    // }

    // RaftRPC Consensus::receivedRequestVoteRPC(RaftRPC req, int serverID) {
    //     std::unique_lock<std::mutex> lock(persistentStateMutex);
    //     RaftRPC resp;
    //     RequestVoteResponse payload;

    //     // If out of date, convert to follower before continuing
    //     if (req.requestvoterequest().term() > currentTerm) {
    //         currentTerm = resp.requestvoterequest().term();
    //         votedFor = -1; // no vote casted in new term
    //         convertToFollower(); 
    //     }

    //     payload.set_term(currentTerm);
    //     if (req.requestvoterequest().term() < currentTerm) {
    //         payload.set_votegranted(false);
    //         resp.set_allocated_requestvoteresponse(&payload);
    //         return resp;
    //     }

    //     if (votedFor == -1 || votedFor == req.requestvoterequest().candidateid()) {
    //         votedFor = req.requestvoterequest().candidateid();
    //         payload.set_votegranted(true);
    //         resetTimer();
    //     } else {
    //         payload.set_votegranted(false);
    //     }
    //     resp.set_allocated_requestvoteresponse(&payload);
    //     return resp;
    // }

    // void Consensus::processRequestVoteRPCResp(RaftRPC resp, int serverID) {
    //     std::unique_lock<std::mutex> lock(persistentStateMutex);
    //     // If out of date, convert to follower and return
    //     if (resp.requestvoteresponse().term() > currentTerm) {
    //         currentTerm = resp.requestvoteresponse().term();
    //         votedFor = -1; // no vote casted in new term
    //         convertToFollower();
    //         return;
    //     }
    //     if (resp.requestvoteresponse().term() == currentTerm && myState == ServerState::CANDIDATE) {
    //         if (resp.requestvoteresponse().votegranted() == true && myVotes.find(serverID) == myVotes.end()) {
    //             numVotesReceived += 1;
    //             myVotes.insert(serverID);
    //             if (numVotesReceived > (config.numServers / 2)) {
    //                 convertToLeader();
    //             }
    //         }
    //     }
    // }

