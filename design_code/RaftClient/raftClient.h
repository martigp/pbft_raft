#include <netinet/in.h>
#include <vector>
#include <socket_testing/src/socket.h>

/**
 * The RaftClient will be initialized with the list of servers and
 * their addresses and will maintain a record of the last leader
 * server they communicated with as well as the socket information
 * to reuse that connection.
 */

class RaftClient {
    public:
        /**
         * @brief Method to send a command
         * Attempts with open leader socket. If it fails, attempt to reopen(Add timeout on this).
         * If the previous leader cannot be contacted or responds that it is 
         * a follower, try with next server on the list. Loop through servers
         * until leader is found and send the 'Entry' RPC.
         * 
         * @param command The string pulled form the command line to send to the leader
        */
        void sendCommand(std::string command);

        /**
         * @brief Read a response from the leader you just sent to
         * TODO: add a timeout on this since the command has failed if the leader fails
         * Can likely just open a simple poll() on the socket to the leader
        */
        std::string readResponse();


    private:
        /**
         * @brief List of RaftServer Addresses
         * TODO: Should we build a little object for server information? name, addr
         * TODO: Finalize how we store this info: servers, number of them, and address
        */
        std::vector<sockaddr_in> raftServerAddrs;

        /***
         *  @brief Number of RaftServers, length of raftServerAddrs
        */
        int numRaftServers;
        
        /**
         * @brief Current ClientSocket object connected to most recent RaftServerLeader
         * Held to maintain client server connection for as long as possible
        */
        ClientSocket currentLeaderSocket;

        /**
         * @brief Some indicator of which server is the leader that corresponds to above socket
         * TODO: Decide how to do this. Is it an integer index in the array of servers? This 
         * could work as we will likely just loop through servers when trying to find the leader.
        */
       int currentLeaderIdx;


};