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
#include "RaftGlobals.hh"
#include "Socket.hh"

#define MAX_CONNECTIONS 1
#define MAX_EVENTS 1
#define CONFIG_PATH "src/server.cfg"

using namespace Raft;

/**
 * @brief Creates a socket that listens for incoming Raft Connections on the
 * server's listed IP address and default raft port. Will exit with failure
 * if there is an issue in setting up the socket.
 * 
 * @return int The file descriptor of the listening socket.
 */
int createListenSocketFd(Raft::Globals& globals) {

    int socketFd;
    struct sockaddr_in listenSockAddr;
    int opt = 1;

    // Creating socket file descriptor
    if ((socketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

 
    // Forcefully attaching socket to the port 8080
    if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Construct listen sockadrr_in from configuration parameters
    listenSockAddr.sin_family = AF_INET;
    listenSockAddr.sin_port = htons(globals.config.raftPort);

    if (inet_pton(AF_INET, globals.config.listenAddr.c_str(),
                              &listenSockAddr.sin_addr) <= 0) {
        perror("Invalid server listen address");
        exit(EXIT_FAILURE);
    }
 
    // Forcefully attaching socket to the port RAFT_PORT
    if (bind(socketFd, (struct sockaddr*)&listenSockAddr,
             sizeof(listenSockAddr))
        < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(socketFd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[Server] listening on %s:%u\n", globals.config.listenAddr.c_str(),
                                          globals.config.raftPort);

    return socketFd;
}

int main(int argc, char const* argv[])
{   
    Raft::Globals globals(CONFIG_PATH);
    int listenSocketFd;
    
    globals.init();

    listenSocketFd = createListenSocketFd(globals);

    Raft::ListenSocket * listenSocket = 
                            new Raft::ListenSocket(listenSocketFd, &globals);

    globals.addkQueueSocket(listenSocket);


    printf("[Server] Set up kqueue with listening socket\n");

    while (1) {
        struct kevent evList[MAX_EVENTS];
        printf("Starting new loop\n");
        
        /* Poll for any events oc*/
        int numEvents = kevent(globals.kq, NULL, 0, evList, MAX_EVENTS, NULL);

        if (numEvents == -1) {
            perror("kevent failure");
            exit(EXIT_FAILURE);
        }

        else if (numEvents == 0) {
            /* No sockets are ready to accepts*/
            printf("[Server] Waiting...\n");
            continue;
        }

        printf("[Server] %d Kqueue events\n", numEvents);

        for (int i = 0; i < numEvents; i++) {

            struct kevent ev = evList[i];
            printf("[Server] Event socket is %d\n", (int) ev.ident);

            Raft::Socket *evSocket = static_cast<Raft::Socket*>(ev.udata);

            if (ev.fflags & EV_EOF) {
                globals.removekQueueSocket(evSocket);
            }
            else {
                evSocket->handleSocketEvent(ev);
            }
        }
    }
}