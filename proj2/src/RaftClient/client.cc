// Simple main function to implement a shell command line
#include <stdio.h>

#include <iostream>
#include <string>

#include "RaftClient/RaftClient.hh"

int main() {
  // Step 1: parse configuration file to know what servers I can communicate
  // with
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
