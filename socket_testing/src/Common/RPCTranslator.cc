#include "Protobuf/RaftRPC.pb.h"
#include "Common/RPCTranslator.hh"

namespace Raft {

char * RPCTranslator::serializeRPCWithHeader(
                                        const Raft::RPC& rpc,
                                        Raft::RPC_Header::MessageType opCode,
                                        size_t *buflen) {
    
    Raft::RPC_Header rpcHeader;

    rpcHeader.set_opcode(opCode);

    size_t payloadLen = rpc.ByteSizeLong();
    rpcHeader.set_payloadlength(payloadLen);

    size_t headerLen = rpcHeader.ByteSizeLong();

    *buflen = headerLen + payloadLen;

    char *buf = new char[*buflen];
    rpcHeader.SerializeToArray(buf, headerLen);
    rpc.SerializeToArray(buf + headerLen, payloadLen);

    return buf;
}

Raft::RPC::Header RPCTranslator::parseRPCHeader(const char *buf,
                                                size_t buflen) {
    Raft::RPC_Header rpcHeader;
    rpcHeader.ParseFromArray(buf, buflen);
    return rpcHeader;
}

bool RPCTranslator::parseRPCMsg(Raft::RPC& rpc, char *buf, size_t len) {
    rpc.ParseFromArray(buf, len);
}

}

