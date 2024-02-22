#ifndef COMMON_KEYVALUESTORAGE_H
#define COMMON_KEYVALUESTORAGE_H

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace Common {

/**
 * @brief Key Value Storage operate similar to a map in C++ where you are able
 * to get and set persistent key:value pairs.
 *
 * Restrictions on values:
 *   -Key must not contain whitespace
 *   -Length of key and value combined may not exceed 256 chars
 *
 */
class KeyValueStorage {
 public:
  /**
   * @brief Exception for the KeyValueStorage class.
   */
  class Exception : public std::runtime_error {
   public:
    Exception(const std::string err) : std::runtime_error(err){};
  };

  /**
   * @brief Opens the file located at the log path or creates a file
   * if not present.
   */
  KeyValueStorage(std::string filepath);

  ~KeyValueStorage();

  /**
   * @brief Sets the value of the specified key. Throws exception if unable to
   * set the value.
   *
   * @param key Key whose value is to be set.
   * @param value Value to set
   */
  void set(std::string key, std::string value);

  /**
   * @brief Gets the value of the specified key. Throws exception if unable to
   * get the value.
   *
   * @param key Key whose value is to be retrieved.
   * @param value returned value.
   */
  void get(std::string key, std::string& value);

 private:
  /**
   * @brief Path of the file opened by this object
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
   * Position for the end of the file if key does not exist.
   */
  std::streampos getLinePosition(std::string key);

};  // class KeyValueStorage
}  // namespace Common

#endif /* COMMON_KEYVALUESTORAGE_H */