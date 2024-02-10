#ifndef RAFT_STORAGE_H
#define RAFT_STORAGE_H

#include <string>

namespace Raft {

    class Storage {
        public:
            /**
             * Create a new Storage object for the Raft Consensus Algorithm
             * Reads from filepath given if exists, else generates a new file
             * 
             * @param storagePath Path for persistent storage file
             * @param firstServerBoot User provided flagindicating whether the
             * server has been run before.
            */
            Storage(const std::string& storagePath, bool firstServerBoot);

            /**
             * Destructor for Storage object
            */
            ~Storage();

            /**
             * @brief Set the value for currentTerm and write to storage
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
             * @brief Set the value for votedFor and write to storage
             * 
             * @param term Value of votedFor
            */
            bool setVotedForValue(uint64_t term);

            /**
             * @brief Get the value for votedFor
             * 
             * @returns Value of the votedFor
            */
            uint64_t getVotedForValue();

            /**
             * @brief Get the value of logLength
             * 
             * @returns Length of the log
            */
            uint64_t getLogLength();

            /**
             * @brief Set the value for lastApplied log index and write to storage
             * 
             * @param term Value of lastApplied
            */
            bool setLastAppliedValue(uint64_t term);

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
             * Locally stored variables for read without accessing storage
            */
            uint64_t currentTerm = 0;
            uint64_t votedFor = 0;
            uint64_t lastApplied = 0;
            uint64_t logLength = 0;
    }; // class Storage
} // namespace Raft

#endif /* RAFT_STORAGE_H */