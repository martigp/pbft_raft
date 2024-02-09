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
#include "RaftServer/RaftServer.hh"

using namespace Raft;

/* Run a Raft Server */
int main(int argc, char const* argv[])
{   
    // Pass in server ID specified on command line and optional flag for first boot
    // TODO: add the optional flag
    Raft::RaftServer server(arvg[1]);
    std::cout << "[RaftServerMain]: in server.cc" << std::endl;

    server.start();
}