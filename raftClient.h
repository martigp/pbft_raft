#include <netinet/in.h>
#include <vector>
#include <socket_testing/src/socket.h>

/**
 * The RaftClient will be responsible for running a simple loop:
 *  (a) read a one-line shell command from standard input
 *  (b) send the command to the cluster leader
 *  (c) print on standard output the results returned by the leader
 * 
 * The RaftClient will be initialized with the list of servers and
 * their addresses and will maintain a record of the last leader
 * server they communicated with as well as the socket information
 * to reuse that connection.
 */

class RaftClient {
    public:


    private:
        /**
         * @brief List of RaftServer Addresses
         * TODO: Should we build a little object for server information? name, addr
         * TODO: Finalize how we store this info: servers, number of them, and address
         * 
        */
        std::vector<sockaddr_in> raftServerAddrs;

        /***
         *  @brief Number of RaftServers, length of raftServerAddrs
         *  
        */
        int numRaftServers;
        
        /**
         * @brief Current ClientSocket object connected to most recent RaftServerLeader
         * Held to maintain client server connection for as long as possible
         * 
        */
        ClientSocket currentLeaderSocket;

        /**
         * @brief Some indicator of which server is the leader that corresponds to above socket
         * TODO: Decide how to do this. Is it an integer index in the array of servers? This 
         * could work as we will likely just loop through servers when trying to find the leader.
         * 
        */
       int currentLeaderIdx;


};