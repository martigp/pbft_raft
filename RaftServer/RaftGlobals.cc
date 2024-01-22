#include <string>
#include <sys/event.h>
#include "RaftGlobals.hh"
#include "socket.hh"

namespace Raft {
    
    Globals::Globals()
    {        
        kq = kqueue();
        if (kq == -1) {
            perror("Failed to create kqueue");
            exit(EXIT_FAILURE);
        }
    }

    Globals::~Globals()
    {
    }

    void Globals::init(std::string configPath) {
        this->configPath = configPath;
        // this->ServerConfig = Common::ServerConfig(this->configPath);
        // this->raftConsensus = Raft::Consensus
    }

    bool Globals::addkQueueSocket(Socket* socket) {
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

    bool Globals::removekQueueSocket(Socket* socket) {
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