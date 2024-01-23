#include <string>
#include <sys/event.h>
#include "SocketManager.hh"
#include "socket.hh"

namespace Raft {
    
    SocketManager::SocketManager()
    {        
        kq = kqueue();
        if (kq == -1) {
            perror("Failed to create kqueue");
            exit(EXIT_FAILURE);
        }
    }

    SocketManager::~SocketManager()
    {
    }

    void SocketManager::init() {
    }

    bool SocketManager::addkQueueSocket(Socket* socket) {
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

    bool SocketManager::removekQueueSocket(Socket* socket) {
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

    IncomingSocketManager::IncomingSocketManager( Raft::Globals& globals, Common::ServerConfig config )
        : SocketManager()
        , globals( globals )
        , config ( config )
    {        
    }

    IncomingSocketManager::~IncomingSocketManager()
    {
    }

    void IncomingSocketManager::init() {
    }

    OutgoingSocketManager::OutgoingSocketManager( Raft::Globals& globals, Common::ServerConfig config )
        : SocketManager()
        , globals( globals )
        , config ( config )
    {        
    }

    OutgoingSocketManager::~OutgoingSocketManager()
    {
    }

    void OutgoingSocketManager::init() {
    }
}