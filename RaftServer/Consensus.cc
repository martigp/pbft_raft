#include <string>
#include "Consensus.hh"

namespace Raft {
    
    Consensus::Consensus( Raft::Globals& globals, ServerConfig config, std::shared_ptr<Raft::LogStateMachine> stateMachine)
        : myState( Consensus::ServerState::FOLLOWER )
        , globals(globals)
        , config(config)
        , stateMachine(stateMachine)
        , numVotesReceived ( 0 )
        , timerTimeout ( 0 )
        , currentTerm ( 0 )
        , votedFor ( -1 )
        , log ( {} )
        , commitIndex ( 0 )
        , lastApplied ( 0 )
        , nextIndex ( 0 )
        , matchIndex ( {} )
    {        
    }

    Consensus::~Consensus()
    {
    }

    Raft::NamedThread Consensus::startTimer() {
        generateRandomElectionTimeout();
        NamedThread timerThread;
        timerThread.myType = NamedThread::ThreadType::TIMER;
        timerThread.thread = std::thread(timerLoop);
        return timerThread;
    }

    void Consensus::timerLoop() {
        while (1) {
            std::unique_lock<std::mutex> lock(resetTimerMutex);
            if (timerResetCV.wait_for(lock, std::chrono::milliseconds(timerTimeout), [&]{ return timerReset == true; })) {
                timerReset = false;
                continue; // TODO: add log, I think this will just restart the loop
            } else {
                timeoutHandler();
            }
        }
    }

    void Consensus::timeoutHandler() {
        switch (myState) {
            case ServerState::FOLLOWER:
                myState = ServerState::CANDIDATE;
                startNewElection();
            case ServerState::CANDIDATE:
                startNewElection();
            case ServerState::LEADER:
                sendAppendEntriesRPCs();
        }
    }

    void Consensus::resetTimer() {
        resetTimerMutex.lock();
        timerReset = true;
        resetTimerMutex.unlock();
        timerResetCV.notify_all();
    }

    void Consensus::generateRandomElectionTimeout() {
        std::random_device seed;
        std::mt19937 gen{seed()}; 
        std::uniform_int_distribution<> dist{5000, 10000};
        timerTimeout = dist(gen);
    }

    void Consensus::setHeartbeatTimeout() {
        timerTimeout = 1000; // TODO: is this right?
    }

    void Consensus::startNewElection() {
        currentTerm += 1;
        votedFor = config.myServerID;
        // TODO: WRITE TO DISK
        numVotesReceived = 1;
        resetTimer();

        RaftRPC req;
        req.set_term(currentTerm);
        req.set_candidateId(config.myServerID);
        req.set_lastLogIndex(0);
        req.set_lastLogTerm(0);
        std::string reqString;
        req.SerializeToString(&reqString);
        globals.broadcastRPC(reqString);
    }

    void Consensus::convertToFollower() {
        myState = ServerState::FOLLOWER;
        generateRandomElectionTimeout();
        resetTimer();    
    }

    void Consensus::convertToLeader() {
        myState = ServerState::LEADER;
        setHeartbeatTimeout();
        resetTimer(); // so not interrupted again 

        // format initial empty AppendEntriesRPC
        RaftRPC req;
        req.set_term(currentTerm);
        req.set_leaderId(config.myServerID);
        req.set_prevLogIndex(0); // will change for proj 2
        req.set_prevLogTerm(0); // will change for proj 2
        req.set_entries({});
        req.set_leaderCommit(0); // will change for proj 2
        globals.broadcastRPC(req);
    }

    void Consensus::sendAppendEntriesRPCs() {
        // format initial empty AppendEntriesRPC
        RaftRPC req;
        req.set_term(currentTerm);
        req.set_leaderId(config.myServerID);
        req.set_prevLogIndex(0); // will change for proj 2
        req.set_prevLogTerm(0); // will change for proj 2
        req.set_entries({});
        req.set_leaderCommit(0); // will change for proj 2
        globals.broadcastRPC(req);
    }

    RaftRPC Consensus::receivedAppendEntriesRPC(RaftRPC req, int serverID); {
        if (req.term() > currentTerm) {
            currentTerm = resp.term();
            votedFor = -1; // no vote casted in new term
            convertToFollower(); // TODO: Do you still try to respond to the AppendEntries in entirety right on conversion to follower?
            // return;
        }

        if (myState == ServerState::Candidate && req.term() == currentTerm) {
            convertToFollower(); // TODO: Do you still try to respond to the AppendEntries in entirety right on conversion to follower?
        }

        RaftRPC resp;
        resp.set_term(currentTerm);
        
        if (req.term() < currentTerm) {
            resp.set_success(false);
            return resp;
        } else {
            // At this point, we must be talking to the currentLeader, resetTimer as specified in Rules for Followe
            resetTimer();

            /* Skipped all of the log replication, dropping soon in Project 2 :P */
            resp.set_success(true);

            return resp;
        }
    }

    void Consensus::processAppendEntriesRPCResp(RaftRPC resp, int serverID) {
        if (resp.term() > currentTerm) {
            currentTerm = resp.term();
            votedFor = -1; // no vote casted in new term
            convertToFollower();
            return;
        }
        /* Do we do anything here for project 1? */
    }

    RaftRPC Consensus::receivedRequestVoteRPC(RaftRPC req, int serverID) {
        if (req.term() > currentTerm) {
            currentTerm = resp.term();
            votedFor = -1; // no vote casted in new term
            convertToFollower();
            return;
        }

        RaftRPC resp;
        resp.set_term(currentTerm);
        if (req.term() < currentTerm) {
            resp.set_voteGranted(false);
            return resp;
        }

        if (votedFor == -1 || votedFor == req.candidateID()) {
            votedFor = req.candidateID();
            resp.set_voteGranted(true);
            resetTimer();
        } else {
            resp.set_voteGranted(false);
        }
        return resp;
    }

    void Consensus::processRequestVoteRPCResp(RaftRPC resp, int serverID) {
        if (resp.term() > currentTerm) {
            currentTerm = resp.term();
            votedFor = -1; // no vote casted in new term
            convertToFollower();
            return;
        }
        if (resp.term() == currentTerm && myState == ServerState::CANDIDATE) {
            if (resp.voteGranted() == true) {
                numVotesReceived += 1;
                if (numVotesReceived > (config.numServers / 2)) {
                    convertToLeader();
                }
            }
        }
    }
}
