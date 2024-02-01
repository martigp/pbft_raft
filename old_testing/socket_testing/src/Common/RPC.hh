#ifndef COMMON_RPC_H
#define COMMON_RPC_H

#include <stddef.h>
#include <sstream>
#include "google/protobuf/message.h"

#define RPC_HEADER_SIZE (size_t) 9

namespace Raft {
    enum RPCType : std::uint8_t {
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

            /**
             * @brief Serialize an RPCHeader into an array.
             * 
             * @param buf Buf to serialize into
             * @param buflen Size of buffer
             */
            void SerializeToArray(char *buf, size_t buflen);

            /**
             * @brief Type of RPC in payload.
             */
            RPCType rpcType;
            /**
             * @brief Size of payload following header.
             */
            size_t payloadLength;
    }; // class RPCHeader

    /**
     * @brief All information required to send an RPC to a peer. This includes
     * an RPCHeader to know what type of RPC it is and how many bytes the RPC
     * is and the actual RPC, encoded in the payload.
     * 
     */
    class RPCPacket {
        public:
            RPCPacket(const RPCHeader& header,
                      const google::protobuf::Message& rpc);
            ~RPCPacket();

            /**
             * @brief The Header for the RPC
             * 
             */
            RPCHeader header;
            /**
             * @brief RPC in byte form.
             * 
             */
            char *payload;
    }; // class RPCPacket
} // namespace Raft

#endif /* COMMON_RPC_H */ 