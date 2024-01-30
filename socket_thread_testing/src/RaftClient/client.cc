// Client side C/C++ program to demonstrate Socket
// programming
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netdb.h>
#include <thread>
#include <vector>
#include <mutex>
#include <iostream>
#include "Protobuf/RaftRPC.pb.h"
#include "Common/RPC.hh"
#include "Common/ClientConfig.hh"

#define CONFIG_PATH "./config_client.cfg"

std::string connectAndSendToServer(Common::ClientConfig config, std::string *in)
{
	int status, readBytes, clientFd;
	struct sockaddr_in serv_addr;
	Raft::RPC::StateMachineCmd::Request stateMachineCmd;
	
	stateMachineCmd.set_allocated_cmd(in);

	size_t payloadLen = stateMachineCmd.ByteSizeLong();

	Raft::RPCHeader header(Raft::RPCType::STATE_MACHINE_CMD, payloadLen);

	char request[RPC_HEADER_SIZE + payloadLen];

	header.SerializeToArray(request, RPC_HEADER_SIZE);
	stateMachineCmd.SerializeToArray(request + RPC_HEADER_SIZE, 
									 payloadLen);

	printf("Sizeof(header)= %lu, Sizeof(payloadlen)=%lu, Sizeof(buffer)=%lu\n", 
			RPC_HEADER_SIZE, sizeof(header.payloadLength), sizeof(request));

	char response[1024] = { 0 };
	if ((clientFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return;
	}

	// RaftClient will continue looping until it has received a response
	while (true) {
		for (auto& it : config.clusterMap) {
			if ((clientFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				printf("\n Socket creation error \n");
				continue;
			}
			if ((status 
				= connect(clientFd, (struct sockaddr*)&serv_addr,
						sizeof(serv_addr)))
				< 0) {
				printf("\nConnection Failed \n");
				close(clientFd);
				continue;
			}
			printf("[Client] Connected to server id %u\n", it.first);
			size_t bytesSent = send(clientFd, request, sizeof(request), 0);
			
			while (true) {
				readBytes = recv(clientFd, response, 1024 - 1, 0);
				if (readBytes > 0 ) {
					if (readBytes > RPC_HEADER_SIZE) {
						Raft::RPCHeader header(response);
						if (readBytes == RPC_HEADER_SIZE + header.payloadLength) {
							Raft::RPC::StateMachineCmd::Response rpcResponse;
							rpcResponse.ParseFromArray(response + RPC_HEADER_SIZE, header.payloadLength);
							if (rpcResponse.success()) {
								return rpcResponse.msg();
							} else {
								break;
							}
						}
					}
				} else if (readBytes == 0) {
					close(clientFd);
					printf("[Client] received no bytes from server\n");
					break;
				} else {
					printf("[Client] Received bad value from server\n");
					perror("Bad Socket");
					close(clientFd);
					break;
				}
			}
			// closing the connected socket
			close(clientFd);
        }
	}
}

int main() {
    // Step 1: parse configuration file to know what servers I can communicate with
    Common::ClientConfig config = Common::ClientConfig(CONFIG_PATH);

    // Step 2: Launch application 
    while (1) {
        std::string str;

        // (a) read line from stdin
        std::getline(std::cin, str);

        // (b) send the command to the cluster leader
        std::string ret = connectAndSendToServer(config, &str);

        // (c) print return value on stdout
        std::cout << ret << std::endl;
    }

    return 0;
}
