#include "KeyValueStorage.hh"

#define MAX_LINE_LEN 258

namespace Common {

KeyValueStorage::KeyValueStorage(std::string filepath) : filepath(filepath) {
  file.open(filepath, std::ios::in | std::ios::out);

  if (!file.is_open()) {
    // to create file, must open it in write mode
    file.open(filepath, std::ios::out);
    // close and reopen in read write mode
    file.close();
    file.open(filepath, std::ios::in | std::ios::out);
  }
}

KeyValueStorage::~KeyValueStorage() {
  file.flush();
  file.close();
}

void KeyValueStorage::set(std::string key, std::string value) {
  file.clear();
  file.seekp(getLinePosition(key));
  std::ostringstream liness;
  // Lines are padded and right justified
  liness << std::right << std::setw(MAX_LINE_LEN - 1) << key + ":" + value
         << "\n";
  std::string linestr = liness.str();
  file << linestr;
  file.flush();

  if (file.fail()) {
    std::string errorMsg = "[KeyValueStorage.cc] Failed to write file " +
                           filepath + " , key-value pair " + key + "-" + value +
                           ": ";
    throw Exception(errorMsg + std::strerror(errno));
  }
}

void KeyValueStorage::get(std::string key, std::string& value) {
  file.clear();
  file.seekp(getLinePosition(key));
  // unable to find the key
  if (file.peek() == EOF) {
    throw Exception("Key " + key + "was not present in storage");
  }
  std::string line;
  getline(file, line);
  value = line.substr(line.find(":") + 1);

  if (file.fail()) {
    std::string errorMsg = "[KeyValueStorage.cc] Failed to read file " +
                           filepath + " , key-value pair " + key + "-" + value +
                           ": ";
    throw Exception(errorMsg + std::strerror(errno));
  }
}

std::streampos KeyValueStorage::getLinePosition(std::string key) {
  std::streampos ret = 0;
  std::string currLine;

  // clear any flags/errors and start at the beginning of the file
  file.clear();
  file.seekg(0);

  // Lines are of form
  // padding || key:value.
  while (std::getline(file, currLine)) {
    // Remove padding from the beginning of the line
    std::string unpaddedLine = currLine.substr(currLine.find_first_not_of(" "));
    if (key == unpaddedLine.substr(0, unpaddedLine.find(":"))) {
      break;
    }
    ret += MAX_LINE_LEN;
  }
  file.clear();
  return ret;
}
}  // namespace Common