#include "Common/RPCNetwork.hh"
#include <sstream>

namespace Raft {

    std::stringstream& operator<<(std::stringstream &ss, RPCHeader header) {
        ss << header.rpcType << header.payloadLength;
        return ss;
    }

    RPCHeader::RPCHeader(RPCType rpcType, size_t payloadLength)
        : rpcType(rpcType),
          payloadLength(payloadLength)
    {
    }

    std::stringstream& operator<<(std::stringstream &ss, RPCPacket packet) {
        ss << packet.header << packet.payload.SerializeAsString();
        return ss;
    }

    RPCHeader::RPCHeader(char *buf) {
        rpcType = (RPCType)*buf;
        payloadLength = (size_t) * (buf + sizeof(rpcType));
    }
    RPCHeader::~RPCHeader()
    {
    }

    std::string RPCHeader::toString() {
        std::stringstream ss;
        std::string rpcHeaderString;

        ss << *this;
        ss >> rpcHeaderString;

        return rpcHeaderString;
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

    std::string RPCPacket::toString() {
        std::stringstream ss;
        std::string rpcPacketString;

        ss << *this;
        ss >> rpcPacketString;

        return rpcPacketString;
    }
}