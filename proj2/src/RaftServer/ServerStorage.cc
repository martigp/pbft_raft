#include "RaftServer/ServerStorage.hh"

#define DIRECTORY_PREFIX "./persistence_raftserver_"
#define PERSISTENT_STATE_FILENAME "/persistent_state.data"
#define LOG_ENTRY_PREFIX "/log_entry_"
#define LOG_ENTRY_SUFFIX ".data"

namespace Raft {
ServerStorage::ServerStorage(uint64_t serverID, bool firstServerBoot) {
  storageDirectory = DIRECTORY_PREFIX + std::to_string(serverID);
  if (std::filesystem::exists(storageDirectory) == false) {
    if (firstServerBoot == true) {
      if (!std::filesystem::create_directory(storageDirectory.c_str())) {
        std::string errorMsg =
            "[ServerStorage.cc] Unable to create persistence directory for "
            "server " +
            std::to_string(serverID) + ": ";
        throw std::runtime_error(errorMsg + std::strerror(errno));
      }
    } else {
      std::string errorMsg =
          "[ServerStorage.cc] On boot, no persistence folder found for "
          "server " +
          std::to_string(serverID) + ": ";
      throw std::runtime_error(errorMsg + std::strerror(errno));
    }
  }

  // If server is restarting(not booting for the first time), it must be able to
  // find it's persistent state file.
  if (firstServerBoot == false &&
      std::filesystem::exists(storageDirectory + PERSISTENT_STATE_FILENAME) ==
          false) {
    std::string errorMsg =
        "[ServerStorage.cc] On boot, no persistent state file found for "
        "server " +
        std::to_string(serverID) + ": ";
    throw std::runtime_error(errorMsg + std::strerror(errno));
  }

  /* Using the generic KeyValueStorage mechanism, the RaftServer Storage
   * manages a KeyValue file for persistent state and for each log entry.
   */
  persistentState.reset(new Common::KeyValueStorage(storageDirectory +
                                                    PERSISTENT_STATE_FILENAME));

  // Set all the initialization values in persistent state
  // OR read in values from an already existing file
  if (firstServerBoot) {
    persistentState->set("currentTerm", std::to_string(0));
    persistentState->set("votedFor", std::to_string(0));
    persistentState->set("lastApplied", std::to_string(0));
  } else {
    // Load from Persistent Storage - requires converting from string then
    // back into uint64_t
    std::string currentTermStr = std::to_string(currentTerm);
    persistentState->get("currentTerm", currentTermStr);
    currentTerm = std::stoull(currentTermStr);

    std::string votedForStr = std::to_string(votedFor);
    persistentState->get("votedFor", votedForStr);
    votedFor = std::stoull(votedForStr);

    std::string lastAppliedStr = std::to_string(lastApplied);
    persistentState->get("lastApplied", lastAppliedStr);
    lastApplied = std::stoull(lastAppliedStr);

    uint64_t logLength = 0;
    for (auto i : std::filesystem::directory_iterator(storageDirectory)) {
      logLength++;
    }
    logLength--;  // subtract out the file that will be it's persistent state

    for (int i = 1; i <= logLength; i++) {
      logEntries.push_back(
          new Common::KeyValueStorage(storageDirectory + LOG_ENTRY_PREFIX +
                                      std::to_string(i) + LOG_ENTRY_SUFFIX));
    }
  }
}

ServerStorage::~ServerStorage() {}

void ServerStorage::setCurrentTermValue(uint64_t term) {
  currentTerm = term;
  persistentState->set("currentTerm", std::to_string(term));
}

uint64_t ServerStorage::getCurrentTermValue() { return currentTerm; }

void ServerStorage::setVotedForValue(uint64_t vote) {
  votedFor = vote;
  persistentState->set("votedFor", std::to_string(vote));
}

uint64_t ServerStorage::getVotedForValue() { return votedFor; }

uint64_t ServerStorage::getLogLength() {
  return logEntries.size();
}

void ServerStorage::setLastAppliedValue(uint64_t index) {
  lastApplied = index;
  persistentState->set("lastApplied", std::to_string(index));
}

uint64_t ServerStorage::getLastAppliedValue() { return lastApplied; }

void ServerStorage::setLogEntry(uint64_t index, uint64_t term,
                                std::string entry) {
  // Allowed to set a new log entry one past the length of the log
  // Implementation of Raft Consensus algorithm guarantees this monotonically
  // increasing property of log entries, thus failures are fatal.
  if (index > getLogLength() + 1) {
    throw std::runtime_error(
        "Error setting log entry, provided index " + std::to_string(index) +
        " was greater the next log index" + std::to_string(getLogLength() + 1));
  } else if (index == getLogLength() + 1) {
    logEntries.push_back(
        new Common::KeyValueStorage(storageDirectory + LOG_ENTRY_PREFIX +
                                    std::to_string(index) + LOG_ENTRY_SUFFIX));
  }
  logEntries[index - 1]->set("term", std::to_string(term));

  logEntries[index - 1]->set("entry", entry);
}

void ServerStorage::getLogEntry(uint64_t index, uint64_t &term,
                                std::string &entry) {
  if (index > getLogLength()) {
    throw Exception("Cannot get log entry with index " + std::to_string(index) +
                    ", " + std::to_string(getLogLength()));
  }
  if (index == 0) {
    throw Exception("Cannot get log entry with index " + std::to_string(index) +
                    ", " + std::to_string(getLogLength()));
  }

  std::string termStr = std::to_string(term);
  logEntries[index - 1]->get("term", termStr);
  term = std::stoull(termStr);

  logEntries[index - 1]->get("entry", entry);
}

// Version of get log entry that only returns the term.
// Provided for the log term checking required when casting votes and 
// deciding commit index.
void ServerStorage::getLogEntry(uint64_t index, uint64_t &term) {
  if (index > getLogLength()) {
    throw Exception("Cannot get log entry with index " + std::to_string(index) +
                    ", " + std::to_string(getLogLength()));
  }
  if (index == 0) {
    throw Exception("Cannot get log entry with index 0");
  }

  std::string termStr = std::to_string(term);
  logEntries[index - 1]->get("term", termStr);
  term = std::stoull(termStr);
}

void ServerStorage::truncateLog(uint64_t index) {
  uint64_t currLength = getLogLength();
  for (int i = currLength; i >= index; i--) {
    // remove from vector of files
    logEntries.erase(logEntries.begin() + index - 1);
    // remove file from directory
    try {
      if (!std::filesystem::remove(storageDirectory + LOG_ENTRY_PREFIX +
                                   std::to_string(i) + LOG_ENTRY_SUFFIX)) {
        std::string errorMsg =
            "[ServerStorage.cc] Unable to remove log entry " +
            std::to_string(i) + " file: ";
        throw std::runtime_error(errorMsg + std::strerror(errno));
      }
    } catch (const std::filesystem::filesystem_error &err) {
      std::string errorMsg =
          "[ServerStorage.cc] Filesystem error, unable to remove log entry " +
          std::to_string(i) + " file: ";
      throw std::runtime_error(errorMsg + err.what());
    }
  }
}
}  // namespace Raft