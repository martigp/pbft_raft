#ifndef COMMON_RPC_TRANSLATOR_H
#define COMMON_RPC_TRANSLATOR_H

#include <string>
#include <vector>
#include "Protobuf/RaftRPC.pb.h"

namespace Raft {
/**
 * @brief Translates RPCs into Arrays and vice versa. Need to parse
 * an RPC header before you can parse the payload as the header encodes
 * the size of the payload and what type of RPC it is.
 */
class RPCTranslator {
    /**
     * @brief Serialize an RPC into bytes.
     * 
     * @param rpc RPC to serialize 
     * @param buflen Size of serialized RPC
     * @return The serialized RPC as an array
     */
    char *serializeRPCWithHeader(const Raft::RPC& rpc,
                                 Raft::RPC_Header::MessageType opCode,
                                 size_t *buflen);

    /**
     * @param buf Array that contain RPC header
     * @param buflen Size of the provided array
     * @return Raft::RPC::Header The parsed RPC header
     */
    Raft::RPC::Header parseRPCHeader(const char *buf, size_t buflen);

    /**
     * @brief Parse an array into an RPC.
     * 
     * @param rpc Returned parsed RPC
     * @param buf Bytes to convert to RPC
     * @param len Number of bytes
     * @return Whether succesffully parsed buffer into an RPC
     */
    bool parseRPCMsg(Raft::RPC& rpc, char *buf, size_t len);
}; // class RPCTranslator
} // namespace Common

#endif /* COMMON_RPC_TRANSLATOR_H */
