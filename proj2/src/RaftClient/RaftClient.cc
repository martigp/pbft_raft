#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netdb.h>
#include <thread>
#include <vector>
#include <mutex>
#include <iostream>
#include "Protobuf/RaftRPC.pb.h"
#include "RaftClient.hh"

namespace Raft {

    RaftClient::RaftClient()
        : config(CONFIG_PATH)
        , network( *this )
    {}

    RaftClient::~RaftClient()
    {}

    // TODO: properly decompose this
    std::string RaftClient::connectAndSendToServer(std::string *in)
    {   
        
    }
}