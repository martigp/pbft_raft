#ifndef COMMON_KEYVALUESTORAGE_H
#define COMMON_KEYVALUESTORAGE_H

#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>

namespace Common {

    class KeyValueStorage {
        public:
            /**
             * Opens the file located at the log path or creates a file if not present
             * TODO: should there be an in memory copy?
            */
            KeyValueStorage(std::string filepath);

            ~KeyValueStorage();

            /**
             * Key Value Storage allows get and set access of a file in memory
             * Converts all types to string for set, converts string based on 
             * return type for get
             * 
             * Operates similar to a map in C++
             * 
             * Set:
             *      If key is not present, creates it and assigns the value
             *      If key is present, overwrites previous value
             *      Returns true for success, false if error occured
             * 
             * Get:
             *      If key is not present, return false
             *      If key is present, return the value
             *      Returns true for success, false if error occured
             * 
             * Restrictions on values:
             *      key must not contain whitespace
             *      length of key and value combined may not exceed 256 chars
             * 
            */

            bool set(std::string key, uint64_t value);

            bool set(std::string key, std::string value);

            bool get(std::string key, std::string& value);
            
            bool get(std::string key, uint64_t& value);

        private:
            /**
             * @brief Filepath opened by this object
            */
            std::string filepath;

            /**
             * @brief File stream used to open file
            */
            std::fstream file;

            /**
             * @brief Seeks file for the line containing the key
             * 
             * @returns Postion of beginning of line where key is stored OR
             * Position for the end of the file to insert a new line
            */
            std::streampos getLinePosition(std::string key);

    }; // class KeyValueStorage
} // namespace Common

#endif /* COMMON_KEYVALUESTORAGE_H */