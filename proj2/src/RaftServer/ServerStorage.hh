#ifndef RAFT_SERVERSTORAGE_H
#define RAFT_SERVERSTORAGE_H

#include <string>
#include <filesystem>
#include <memory>
#include <sys/stat.h>
#include "Common/KeyValueStorage.hh"

namespace Raft {

    class ServerStorage {
        public:
            /**
             * Create a new ServerStorage object for the Raft Consensus Algorithm
             * Using the generic KeyValueStorage mechanism, the RaftServer Storage
             * manages a KeyValue file for persistent state and for each log entry.
             * 
             * @param serverID ID of RaftServer used tagging its persistence files
             * 
             * @param firstServerBoot User provided flag indicating whether the
             * server has been run before.
            */
            ServerStorage(uint64_t serverID, bool firstServerBoot);

            /**
             * Destructor for ServerStorage object
            */
            ~ServerStorage();

            /**
             * @brief Set the value for currentTerm and write to ServerStorage
             * 
             * @param term Value of the currentTerm
            */
            bool setCurrentTermValue(uint64_t term);

            /**
             * @brief Get the value for currentTerm
             * 
             * @returns Value of the currentTerm
            */
            uint64_t getCurrentTermValue();

            /**
             * @brief Set the value for votedFor and write to ServerStorage
             * 
             * @param votedFor Value of votedFor
            */
            bool setVotedForValue(uint64_t votedFor);

            /**
             * @brief Get the value for votedFor
             * 
             * @returns Value of the votedFor
            */
            uint64_t getVotedForValue();

            /**
             * @brief Iterate the Server Storage directory to get the length of the log
             * 
             * @returns Length of the log
            */
            uint64_t getLogLength();

            /**
             * @brief Set the value for lastApplied log index and write to ServerStorage
             * 
             * @param index Value of lastApplied
            */
            bool setLastAppliedValue(uint64_t index);

            /**
             * @brief Get the value for lastApplied log index
             * 
             * @returns Value of the lastApplied
            */
            uint64_t getLastAppliedValue();

            /**
             * @brief Store a log entry at a specified index with a specified term
             * 
             * @param index Index in the log
             * 
             * @param term Term for the log entry
             * 
             * @param entry String contents of the entry, which is a shell command in
             *              our specific use case
             * 
             * @returns boolean indicating success or failure
            */
            bool setLogEntry(uint64_t index, uint64_t term, std::string entry);

            /**
             * @brief Get a log entry at a specified index
             * 
             * @param index Index in the log
             * 
             * @param term Term of the entry, pass out by reference
             * 
             * @param entry String contents of the entry, pass out by reference
             * 
             * @returns boolean indicating success or failure
            */
            bool getLogEntry(uint64_t index, uint64_t &term, std::string &entry);

            /**
             * @brief Truncate log entries starting at specified index
             * 
             * @param index Index in the log
             * 
             * @returns boolean indicating success or failure. If index is not 
             *          within the log, will return success.
            */
            bool truncateLog(uint64_t index);

        private:
            /**
             * @brief Server storage directory path
            */
            std::string storageDirectory;

            /**
             * Locally stored variables for read without accessing ServerStorage
            */
            uint64_t currentTerm = 0;
            uint64_t votedFor = 0;
            uint64_t lastApplied = 0;

            /**
             * Generic Key-Value storage objects used for both the state and 
             * each log entry
            */
            std::unique_ptr<Common::KeyValueStorage> persistentState;
            std::vector<Common::KeyValueStorage*> logEntries = {};

    }; // class ServerStorage
} // namespace Raft

#endif /* RAFT_SERVERSTORAGE_H */