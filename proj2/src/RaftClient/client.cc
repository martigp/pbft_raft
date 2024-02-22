// Simple main function to implement a replicated shell command line
// applications
#include <stdio.h>

#include <iostream>
#include <string>

#include "RaftClient/RaftClient.hh"

int main() {
  // Step 1: Generate instance of RaftClient
  //         See RaftClient Interface for required configuration
  Raft::RaftClient raftClient = Raft::RaftClient();

  // Step 2: Launch application
  while (1) {
    std::string *cmd = new std::string();
    // (a) Prompt client for input
    std::cout << "Enter shell command:" << std::endl;

    // (b) read line from stdin
    std::getline(std::cin, *cmd);

    // (c) send the command to the cluster leader
    std::string ret = raftClient.sendToServer(cmd);

    // (d) print return value on stdout
    std::cout << ret << std::endl;
  }

  return 0;
}
