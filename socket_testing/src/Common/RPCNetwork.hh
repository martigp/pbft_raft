#ifndef COMMON_RPCNETWORK_H
#define COMMON_RPCNETWORK_H

#include <stddef.h>
#include <sstream>
#include "google/protobuf/message.h"

namespace Raft {
    enum RPCType {
        APPEND_ENTRIES,
        REQUEST_VOTE,
        STATE_MACHINE_CMD,
        NONE
    };

    /**
     * @brief Provides information on RPC being sent by peer over a socket.
     * 
     */
    class RPCHeader {
        public:
            /**
             * @brief Constructor
             * 
             * @param rpcType
             * @param payloadLength
             */
            RPCHeader(RPCType rpcType, size_t payloadLength);

            /**
             * @brief Construct a new RPCHeader from a buffer
             * 
             * @param buf 
             */
            RPCHeader(char *buf);

            /**
             * @brief Destroy the RPCHeader object
             * 
             */
            ~RPCHeader();

            friend std::stringstream& operator<<(std::stringstream &ss,
                                            RPCHeader header);

            std::string toString();

            /**
             * @brief Type of RPC in payload.
             */
            RPCType rpcType;
            /**
             * @brief Size of payload following header.
             */
            size_t payloadLength;
    }; // class RPCHeader

    class RPCPacket {
        public:
            RPCPacket(const RPCHeader& header,
                      const google::protobuf::Message& payload);
            ~RPCPacket();

            friend std::stringstream& operator<<(std::stringstream &ss,
                                                 RPCPacket packet);

            std::string toString();

            RPCHeader header;
            char *payload;
    }; // class RPCNetworkPacket
} // namespace Raft

#endif /* COMMON_RPCNETWORK_H */ 