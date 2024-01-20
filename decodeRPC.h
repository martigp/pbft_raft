#include <string>
#include <vector>
// include protobuf methods for decode

/**
 * These two methods serve to decode the incoming data
 * read off of a file descriptor. These methods abstract 
 * away the complications involved with receiving requests,
 * namely multiple requests and incomplete/incorrectly formatted.
*/


/**
 * @brief This method is used to transform a char buffer
 * into an RPC request packet. Since each RaftServer
 * has a server type socket for listening/receiving requests, 
 * this method will be invoked and can decode knowing it will 
 * only be receiving RPCs of the request format.
 * 
 * TODO: converse and make sure that this will only ever be one request
 * I believe a server can receiver multiple requests, but they will be on different ports,
 * so different events in queue, so processed and read separately
 * See raftSever.h. Because if the state machine had to process a bunch of requests how would just
 * a true/false success readinging back to other server be able to decode that. 
 * TODO: think about super delayed responses(how to know which request it's from)
 * TODO: will you have multiple outstanding requests from the same server and how to respond separately(ties into above)
 * 
 * @param buf The char buffer returned from recv on the 
 * server socket.
 * 
 * @param bytesRead The length of bytes read from the recv 
 * method on the server socket
 * 
 * @return array of valid, formatted RPC requests
 */
message decodeRequestRecv(char * buf, size_t bytesRead);

/**
 * @brief This method is used to transform a char buffer
 * into separate RPC response packets. Since each RaftServer
 * and RaftClient has a client type socket for sending requests, 
 * this method will be invoked and can decode knowing it will 
 * only be receiving RPCs of the response format, in response to 
 * requests it has initiated.
 * 
 * @param buf The char buffer returned from recv on the 
 * client socket.
 * 
 * @param bytesRead The length of bytes read from the recv 
 * method on the client socket
 * 
 * @return array of valid, formatted RPC reponses
 */


std::vector<message> decodeResponseRecv(char * buf, size_t bytesRead);

