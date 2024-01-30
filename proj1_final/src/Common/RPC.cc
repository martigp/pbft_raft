#include "Common/RPC.hh"
#include <sstream>

namespace Raft {

    RPCHeader::RPCHeader()
        : rpcType(RPCType::NONE),
          payloadLength(0)
        {}
    
    RPCHeader::RPCHeader(RPCType rpcType, size_t payloadLength)
        : rpcType(rpcType),
          payloadLength(payloadLength)
    {
    }

    RPCHeader::RPCHeader(char *buf) {
        rpcType = (RPCType)*buf;
        payloadLength = (size_t) * (buf + sizeof(rpcType));
    }
    RPCHeader::~RPCHeader()
    {
    }

    void RPCHeader::SerializeToArray(char *buf, size_t buflen) {

        assert(buflen == RPC_HEADER_SIZE);
        
        memcpy(buf, &rpcType, sizeof(Raft::RPCType));
        memcpy(buf + sizeof(Raft::RPCType), (&payloadLength), sizeof(uint64_t));
    }

    RPCPacket::RPCPacket(const RPCHeader& header, const google::protobuf::Message& rpc) 
        : header(header)
    {
        payload = new char[rpc.ByteSizeLong()];
        rpc.SerializeToArray(payload, sizeof(payload));
    }

    RPCPacket::~RPCPacket()
    {
    }
}