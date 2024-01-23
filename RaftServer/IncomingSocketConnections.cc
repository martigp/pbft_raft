#include <string>
#include <sys/event.h>
#include "RaftIncomingSocketConnections.hh"
#include "socket.hh"

namespace Raft {
    
    IncomingSocketConnections::IncomingSocketConnections()
        : config()
    {        
        kq = kqueue();
        if (kq == -1) {
            perror("Failed to create kqueue");
            exit(EXIT_FAILURE);
        }
    }

    IncomingSocketConnections::~IncomingSocketConnections()
    {
    }

    void IncomingSocketConnections::init(ServerConfig config) {
        this->config = configPath;
        listenSocketFd = createListenSocketFd();

        printf("[Server] Listening on port %d\n", RAFT_PORT);

        Raft::ListenSocket * listenSocket = 
                                new Raft::ListenSocket(listenSocketFd, &globals);

        globals.addkQueueSocket(listenSocket);


        printf("[Server] Set up kqueue with listening socket\n");
    }

    void IncomingSocketConnections::start() {

    }

    bool IncomingSocketConnections::addkQueueSocket(Socket* socket) {
        struct kevent ev;

        /* Set flags in event for kernel to notify when data arrives
           on socket and the udata pointer to the data identifier.  */
        EV_SET(&ev, socket->fd, EVFILT_READ, EV_ADD, 0, 0, socket);

        if (kevent(kq, &ev, 1, NULL, 0, NULL) == -1) {
            perror("Failure to register client socket");
            return false;
        }

        return true;
    }

    bool IncomingSocketConnections::removekQueueSocket(Socket* socket) {
        struct kevent ev;

        /* Set flags for an event to stop the kernel from listening for
        events on this socket. */
        EV_SET(&ev, socket->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);

        if (kevent(kq, &ev, 1, NULL, 0, NULL) == -1) {
            perror("Failure to register client socket");
            return false;
        }

        delete socket;

        return true;
    }
}