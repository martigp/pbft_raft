#include <RaftServer/ServerStorage.hh>

#define DIRECTORY_PREFIX "./persistence_raftserver_"
#define PERSISTENT_STATE_FILENAME "/persistent_state.data"
#define LOG_ENTRY_PREFIX "/log_entry_"
#define LOG_ENTRY_SUFFIX ".data"

namespace Raft {
    ServerStorage::ServerStorage(uint64_t serverID, bool firstServerBoot) {
        printf("value of first server boot: %d", firstServerBoot);
        storageDirectory = DIRECTORY_PREFIX + std::to_string(serverID);
        if (std::filesystem::exists(storageDirectory) == false) {
            if (firstServerBoot == true) {
                if (!std::filesystem::create_directory(storageDirectory.c_str())) {
                    std::string errorMsg = "[ServerStorage.cc] Unable to create persistence directory for server " + std::to_string(serverID) + ": ";
                    throw std::runtime_error(errorMsg + std::strerror(errno));
                } 
            } else {
                std::string errorMsg = "[ServerStorage.cc] On boot, no persistence folder found for server " + std::to_string(serverID) + ": ";
                throw std::runtime_error(errorMsg + std::strerror(errno));
            }
        }

        if (firstServerBoot == false && std::filesystem::exists(storageDirectory + PERSISTENT_STATE_FILENAME) == false) {
            std::string errorMsg = "[ServerStorage.cc] On boot, no persistent state file found for server " + std::to_string(serverID) + ": ";
            throw std::runtime_error(errorMsg + std::strerror(errno));
        }

        persistentState.reset(new Common::KeyValueStorage(storageDirectory + PERSISTENT_STATE_FILENAME));

        // Set all the initialization values in persistent state
        // OR read in values from an already existing file
        if (firstServerBoot) {
            persistentState->set("currentTerm", 0);
            persistentState->set("votedFor", 0);
            persistentState->set("lastApplied", 0);
        } else {
            persistentState->get("currentTerm", currentTerm);
            persistentState->get("votedFor", votedFor);
            persistentState->get("lastApplied", lastApplied);

            uint64_t logLength = 0;
            for (auto i: std::filesystem::directory_iterator(storageDirectory)) {
                logLength++;
            }
            logLength--; // subtract out the file that will be it's persistent state
            
            for (int i = 1; i <= logLength; i++) {
                logEntries.push_back(new Common::KeyValueStorage(storageDirectory + LOG_ENTRY_PREFIX + std::to_string(i) + LOG_ENTRY_SUFFIX));
            }
        }
    }

    ServerStorage::~ServerStorage() {
    }

    bool ServerStorage::setCurrentTermValue(uint64_t term) {
        currentTerm = term;
        return persistentState->set("currentTerm", term);
    }

    uint64_t ServerStorage::getCurrentTermValue() {
        return currentTerm;
    }

    bool ServerStorage::setVotedForValue(uint64_t vote) {
        votedFor = vote;
        return persistentState->set("votedFor", vote);
    }

    uint64_t ServerStorage::getVotedForValue() {
        return votedFor;
    }

    uint64_t ServerStorage::getLogLength() {
        return logEntries.size();
    }

    bool ServerStorage::setLastAppliedValue(uint64_t index) {
        lastApplied = index;
        return persistentState->set("lastApplied", index);
    }

    uint64_t ServerStorage::getLastAppliedValue() {
        return lastApplied;
    }

    bool ServerStorage::setLogEntry(uint64_t index, uint64_t term, std::string entry) {
        // allowed to set a new log entry one past the length of the log
        if (index > getLogLength() + 1) {
            return false;
        } else if (index == getLogLength() + 1){
            logEntries.push_back(new Common::KeyValueStorage(storageDirectory + LOG_ENTRY_PREFIX + std::to_string(index) + LOG_ENTRY_SUFFIX));
        }
        return logEntries[index - 1]->set("term", term) && logEntries[index - 1]->set("entry", entry);
    }

    bool ServerStorage::getLogEntry(uint64_t index, uint64_t &term, std::string &entry) {
        if (index > getLogLength()) {
            return false;
        }
        // TODO: error checking here
        return logEntries[index - 1]->get("term", term) && logEntries[index - 1]->get("entry", entry);
    }

    bool ServerStorage::truncateLog(uint64_t index) {
        uint64_t currLength = getLogLength();
        for (int i = currLength; i >= index; i--) {
            // remove from vector of files, TODO: does this call the destructor?
            logEntries.erase(logEntries.begin() + index - 1);
            // remove file from directory
            try {
                if (!std::filesystem::remove(storageDirectory + LOG_ENTRY_PREFIX + std::to_string(i) + LOG_ENTRY_SUFFIX)) {
                    std::string errorMsg = "[ServerStorage.cc] Unable to remove log entry " + std::to_string(i) + " file: ";
                    throw std::runtime_error(errorMsg + std::strerror(errno));
                }
            } catch(const std::filesystem::filesystem_error& err) {
                std::string errorMsg = "[ServerStorage.cc] Filesystem error, unable to remove log entry " + std::to_string(i) + " file: ";
                throw std::runtime_error(errorMsg + err.what());
            }
        }
        return true;
    }
}