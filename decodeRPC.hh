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
 * into separate RPC request packets. Since each RaftServer
 * has a server type socket for listening/receiving requests, 
 * this method will be invoked and can decode knowing it will 
 * only be receiving RPCs of the request format.
 * 
 * @param buf The char buffer returned from recv on the 
 * server socket.
 * 
 * @param bytesRead The length of bytes read from the recv 
 * method on the server socket
 * 
 * @return array of valid, formatted RPC requests
 */


vector<message> decodeRequestRecv(char * buf, size_t bytesRead);

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


vector<message> decodeResponseRecv(char * buf, size_t bytesRead);

