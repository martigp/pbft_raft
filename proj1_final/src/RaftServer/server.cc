#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/event.h>
#include <sys/time.h>
#include <memory>
#include <fstream>
#include <filesystem>
#include <string>
#include "RaftServer/RaftGlobals.hh"
#include "RaftServer/Socket.hh"

#define CONFIG_PATH_HEADER "./config_ID_"
#define CONFIG_SUFFIX ".cfg"

using namespace Raft;

/* Run a Raft Server */
int main(int argc, char const* argv[])
{   
    Raft::Globals globals(CONFIG_PATH_HEADER + std::string(argv[1]) + CONFIG_SUFFIX);
    std::cout << "[RaftServerMain]: in server.cc" << std::endl;

    globals.start();
}