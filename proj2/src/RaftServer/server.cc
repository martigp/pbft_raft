#include <iostream>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "RaftServer/RaftServer.hh"

using namespace Raft;


void printUsage() {
    std::cout << "usage: server [<options>] <config_path>" << std::endl
              << "      -n  create Raft Server for first time that "
                 "does not have any persistent storage." << std::endl
              << "      -h  show usage" << std::endl;
}

/* Run a Raft Server */
int main(int argc, char *argv[])
{   
    // Pass in server ID specified on command line and optional flag for first boot
    // TODO: add the optional flag

    int opt;
    bool firstServerBoot = false;

    while((opt = getopt(argc, argv, "hn")) != -1) {
        switch(opt) {
            case 'h':
                std::cout << "Got h" << std::endl;
                printUsage();
                return 0;
            case 'n':
                std::cout << "Got n" << std::endl;
                firstServerBoot = true;
                break;
            default:
                std::cerr << "Received erroneous option with value " << (char) optopt
                          << std::endl;
                return 1;
        }
    }

    if (optind != argc - 1) {
        std::cout << "Optind is " << optind << std::endl;
        std::cout << "Too is too many arguments" << std::endl;
        printUsage();
        return 1;
    }

    // Bypass getopt for now
    firstServerBoot = true;

    Raft::RaftServer server(argv[optind], firstServerBoot);
    std::cout << "[RaftServerMain]: in server.cc" << std::endl;

    server.start();
}