#include "Consensus.hh"

namespace Raft {
    
    Consensus::Consensus( Raft::Globals& globals)
        : globals ( globals )
        , myState ( Consensus::ServerState::FOLLOWER )
        , serverId ( globals.config.serverId )
        , leaderId ( 0)
        , currentTerm ( 0 )
        , votedFor ( 0 )
        , log ( {} )
        , commitIndex ( 0 )
        , lastApplied ( 0 )
        , nextIndex ( 0 )
        , matchIndex ( {} )
        , mostRecentRequestId ( 0 )
        , timerTimeout ( 0 )
        , timerReset ( false )
        , numVotesReceived ( 0 )
        , myVotes ( {} )
    {        
    }

    Consensus::~Consensus()
    {
    }

    void Consensus::startTimer(std::thread &timerThread) {
        generateRandomElectionTimeout();
        timerThread = std::thread(&Consensus::timerLoop, this);
    }

    void Consensus::loadPersistentState() {
        printf("[Consensus]: Loading persistent state\n");
        PersistentState state;
        std::ifstream file;
        // TODO: make a subdirector string formatter
        if (!std::filesystem::exists("./persistentRaftServerState_ID_" + std::to_string(serverId) + "/")) {
            std::filesystem::create_directory("./persistentRaftServerState_ID_" + std::to_string(serverId) + "/");
        } 
        file.open("./persistentRaftServerState_ID_" + std::to_string(serverId) + "/PersistentState.txt", std::ios::in);
        if (file.is_open()) {
            file.seekg(0);
            file.read((char*)&state, sizeof(state));
            currentTerm = state.term;
            votedFor = state.votedFor;
        } else {
            currentTerm = 0;
            votedFor = 0;
        }
    }

    void Consensus::writePersistentState() {
        printf("[Consensus]: Writing persistent state\n");
        PersistentState state;
        state.term = currentTerm;
        state.votedFor = votedFor;
        std::ofstream file;
        if (!std::filesystem::exists("./persistentRaftServerState_ID_" + std::to_string(serverId) + "/")) {
            std::filesystem::create_directory("./persistentRaftServerState_ID_" + std::to_string(serverId) + "/");
        }
        file.open("./persistentRaftServerState_ID_" + std::to_string(serverId) + "/PersistentState.txt", std::ios::trunc);
        file.write((char*)&state, sizeof(state));
        file.close();
    }

    void Consensus::handleRaftClientReq(uint64_t peerId, Raft::RPCHeader header, char *payload) {
        printf("[Consensus]: handleRaftClientReq\n");
        if (myState != ServerState::LEADER) {
            Raft::RPC::StateMachineCmd::Response resp;
            // TODO: decide on format of failure
            resp.set_success(false);
            resp.set_leaderid(leaderId);
            resp.set_msg("");
            globals.serverSocketManager->sendRPC(peerId, resp, RPCType::STATE_MACHINE_CMD);
        } else {
            Raft::RPC::StateMachineCmd::Request rpc;
            rpc.ParseFromArray(payload, header.payloadLength);
            globals.logStateMachine->pushCmd({peerId, rpc.cmd()});
        }
    }

    void Consensus::handleRaftServerReq(uint64_t peerId, Raft::RPCHeader header, char *payload) {
        printf("[Consensus]: handleRaftServerReq\n");
        if (header.rpcType == RPCType::APPEND_ENTRIES) {
            Raft::RPC::AppendEntries::Request req;
            req.ParseFromArray(payload, header.payloadLength);
            receivedAppendEntriesRPC(req, peerId);
        } else if (header.rpcType == RPCType::REQUEST_VOTE) {
            Raft::RPC::RequestVote::Request req;
            req.ParseFromArray(payload, header.payloadLength);
            receivedRequestVoteRPC(req, peerId);
        } else {
            // TODO: add log or error handling here
            std::cout << "Invalid Request" << std::endl;
        }
    }

    void Consensus::handleRaftServerResp(uint64_t peerId, Raft::RPCHeader header, char *payload) {
        printf("[Consensus]: handleRaftServerResp\n");
        if (header.rpcType == RPCType::APPEND_ENTRIES) {
            Raft::RPC::AppendEntries::Response resp;
            resp.ParseFromArray(payload, header.payloadLength);
            processAppendEntriesRPCResp(resp, peerId);
        } else if (header.rpcType == RPCType::REQUEST_VOTE) {
            Raft::RPC::RequestVote::Response resp;
            resp.ParseFromArray(payload, header.payloadLength);
            processRequestVoteRPCResp(resp, peerId);
        } else {
            // TODO: add log or error handling here
            std::cout << "Invalid Response" << std::endl;
        }
    }

    void Consensus::timerLoop() {
        printf("[Consensus]: Beginning timer loop\n");
        while (true) {
            std::unique_lock<std::mutex> lock(resetTimerMutex);
            if (timerResetCV.wait_for(lock, std::chrono::milliseconds(timerTimeout), [&]{ return timerReset == true; })) {
                timerReset = false;
                printf("[Consensus]: Timer reset, state: %d\n", myState);
                lock.unlock();
                continue; // TODO: add log, I think this will just restart the loop
            } else {
                printf("[Consensus]: Timed Out, state: %d\n", myState);
                lock.unlock();
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
        std::unique_lock<std::mutex> lock(resetTimerMutex);
        timerReset = true;
        timerResetCV.notify_all();
    }

    void Consensus::generateRandomElectionTimeout() {
        std::unique_lock<std::mutex> lock(resetTimerMutex);
        std::random_device seed;
        std::mt19937 gen{seed()}; 
        std::uniform_int_distribution<> dist{10000, 20000};
        timerTimeout = dist(gen);
        printf("[Consensus]: New timer timeout: %llu\n", timerTimeout);
    }

    void Consensus::setHeartbeatTimeout() {
        std::unique_lock<std::mutex> lock(resetTimerMutex);
        timerTimeout = 1000; // TODO: is this right?
    }

    void Consensus::startNewElection() {
        std::unique_lock<std::mutex> lock(consensusMutex);
        printf("[Consensus]: Start New Election\n");
        currentTerm += 1;
        votedFor = serverId;
        writePersistentState();
        myVotes.clear();
        myVotes.insert(serverId);
        numVotesReceived = 1;
        resetTimer();

        Raft::RPC::RequestVote::Request req;
        req.set_term(currentTerm);
        req.set_candidateid(serverId);
        req.set_lastlogindex(0);
        req.set_lastlogterm(0);
        printf("[Consensus]: About to send RequestVote, term: %llu, serverId: %llu\n", currentTerm, serverId);
        for (auto& peer: globals.config.clusterMap) {
            globals.clientSocketManager->sendRPC(peer.first, req, RPCType::REQUEST_VOTE);
        }
    }

    void Consensus::convertToFollower() {
        myState = ServerState::FOLLOWER;
        generateRandomElectionTimeout();
        resetTimer();    
    }

    void Consensus::convertToLeader() {
        printf("[Consensus]: Convert to leader");
        myState = ServerState::LEADER;
        setHeartbeatTimeout();
        resetTimer(); // so not interrupted again 

        // This is OK for now as sendAppenEntriesRPCs is always just empty
        sendAppendEntriesRPCs();
    }

    void Consensus::sendAppendEntriesRPCs() {
        printf("[Consensus]: Send Append Entries\n");
        Raft::RPC::AppendEntries::Request req;
        req.set_term(currentTerm);
        req.set_leaderid(serverId);
        req.set_leadercommit(0);
        req.set_prevlogindex(0);
        req.set_prevlogterm(0);
        req.add_entries();
        for (auto& peer: globals.config.clusterMap) {
            mostRecentRequestId[peer.first]++;
            req.set_requestid(mostRecentRequestId[peer.first]);
            globals.clientSocketManager->sendRPC(peer.first, req, RPCType::APPEND_ENTRIES);
        }
    }

    void Consensus::receivedAppendEntriesRPC(Raft::RPC::AppendEntries::Request req, int peerId) {
        std::unique_lock<std::mutex> lock(consensusMutex);
        printf("[Consensus]: Received Append Entries\n");
        Raft::RPC::AppendEntries::Response resp;
        // If out of date, convert to follower before continuing
        if (req.term() > currentTerm) {
            currentTerm = req.term();
            votedFor = 0; // no vote casted in new term
            convertToFollower();
        }

        // Currently running an election, AppendEntries from new term leader, convert to follower and continue
        if (myState == ServerState::CANDIDATE && req.term() == currentTerm) {
            convertToFollower(); 
        }

        // Include requestID in response for RPC pairing
        resp.set_term(currentTerm);
        resp.set_requestid(req.requestid());
        
        if (req.term() < currentTerm) {
            resp.set_success(false);
        } else {
            // At this point, we must be talking to the currentLeader, resetTimer as specified in Rules for Follower
            resetTimer();

            // Update who the current leader is
            leaderId = peerId;

            /** Skipped all of the log replication
             * Dropping soon in Project 2 :P
             * Leaving some space here as a mental marker :)
            */
            resp.set_success(true);
        }
        globals.serverSocketManager->sendRPC(peerId, resp, RPCType::APPEND_ENTRIES);
    }

    void Consensus::processAppendEntriesRPCResp(Raft::RPC::AppendEntries::Response resp, int peerId) {
        std::unique_lock<std::mutex> lock(consensusMutex);
        printf("[Consensus]: Process Append Entries Response\n");
        // If out of date, convert to follower before continuing
        if (resp.term() > currentTerm) {
            currentTerm = resp.term();
            votedFor = 0; // no vote casted in new term
            convertToFollower();
        }
        /* Do we do anything here for project 1? */
        // ignore if it is not the response to our msot recent request
        if (resp.requestid() != mostRecentRequestId[peerId]) {
            return;
        }

        /** Skipped all of the log replication
         * Dropping soon in Project 2 :P
         * Leaving some space here as a mental marker :)
        */
    }

    void Consensus::receivedRequestVoteRPC(Raft::RPC::RequestVote::Request req, int peerId) {
        std::unique_lock<std::mutex> lock(consensusMutex);
        printf("[Consensus]: Received Request Vote\n");
        Raft::RPC::RequestVote::Response resp;

        // If out of date, convert to follower before continuing
        if (req.term() > currentTerm) {
            currentTerm = req.term();
            votedFor = 0; // no vote casted in new term
            convertToFollower(); 
        }

        resp.set_term(currentTerm);
        if (req.term() < currentTerm) {
            resp.set_votegranted(false);
        } else if (votedFor == 0 || votedFor == req.candidateid()) {
            votedFor = req.candidateid();
            resp.set_votegranted(true);
            resetTimer();
        } else {
            resp.set_votegranted(false);
        }
        globals.serverSocketManager->sendRPC(peerId, resp, RPCType::REQUEST_VOTE);
    }

    void Consensus::processRequestVoteRPCResp(Raft::RPC::RequestVote::Response resp, int peerId) {
        std::unique_lock<std::mutex> lock(consensusMutex);
        printf("[Consensus]: Process Request Vote Response\n");
        // If out of date, convert to follower and return
        // TODO: can't happen here though?
        if (resp.term() > currentTerm) {
            currentTerm = resp.term();
            votedFor = 0; // no vote casted in new term
            convertToFollower();
            return;
        }
        if (resp.term() == currentTerm && myState == ServerState::CANDIDATE) {
            if (resp.votegranted() == true && myVotes.find(peerId) == myVotes.end()) {
                numVotesReceived += 1;
                myVotes.insert(peerId);
                if (numVotesReceived > (globals.config.clusterMap.size() / 2)) {
                    convertToLeader();
                }
            }
        }
    }
}
