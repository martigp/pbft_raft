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
            */
            Storage(std::string storagePath);

            /**
             * Destructor for Storage object
            */
            ~Storage();

            /**
             * @brief Set the value for currentTerm and write to storage
             * 
             * @param term Value of the currentTerm
            */
            bool setCurrentTermValue(int32_t term);

            /**
             * @brief Get the value for currentTerm
             * 
             * @returns Value of the currentTerm
            */
            int32_t getCurrentTermValue();

            /**
             * @brief Set the value for votedFor and write to storage
             * 
             * @param term Value of votedFor
            */
            bool setVotedForValue(int32_t term);

            /**
             * @brief Get the value for votedFor
             * 
             * @returns Value of the votedFor
            */
            int32_t getVotedForValue();

            /**
             * @brief Get the value of logLength
             * 
             * @returns Length of the log
            */
            int32_t getLogLength();

            /**
             * @brief Set the value for lastApplied log index and write to storage
             * 
             * @param term Value of lastApplied
            */
            bool setLastAppliedValue(int32_t term);

            /**
             * @brief Get the value for lastApplied log index
             * 
             * @returns Value of the lastApplied
            */
            int32_t getLastAppliedValue();

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
            bool setLogEntry(int32_t index, int32_t term, std::string entry);

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
            bool getLogEntry(int32_t index, int32_t &term, std::string &entry);

            /**
             * @brief Truncate log entries starting at specified index
             * 
             * @param index Index in the log
             * 
             * @returns boolean indicating success or failure. If index is not 
             *          within the log, will return success.
            */
            bool truncateLog(int32_t index);


        private:

            /**
             * Locally stored variables for read without accessing storage
            */
            int32_t currentTerm = -1;
            int32_t votedFor = -1;
            int32_t lastApplied = -1;
            int32_t logLength = -1;
    }; // class Storage
} // namespace Raft

#endif /* RAFT_STORAGE_H */