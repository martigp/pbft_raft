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
#include "Protobuf/RaftRPC.pb.h"
#include "Common/RPC.hh"

#define PORT 1234
#define NUM_THREADS 1

void connectAndSendToServer(int tid)
{
restart:
	int status, readBytes, clientFd;
	struct sockaddr_in serv_addr;
	Raft::RPC::StateMachineCmd::Request stateMachineCmd;
	
	std::string * cmdPtr = new std::string("pwd");
	stateMachineCmd.set_allocated_cmd(cmdPtr);

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

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary
	// form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)
		<= 0) {
		printf(
			"\nInvalid address/ Address not supported \n");
		return;
	}

	if ((status
		= connect(clientFd, (struct sockaddr*)&serv_addr,
				sizeof(serv_addr)))
		< 0) {
		printf("\nConnection Failed \n");
		close(clientFd);
		sleep(5);
		goto restart;
	}
	printf("[Client %d] Connected to server\n", tid);
	Raft::RPC_StateMachineCmd_Request test;
	test.ParseFromArray(request + RPC_HEADER_SIZE, header.payloadLength);

	printf("[Client] About to send cmd %s\n", test.cmd().c_str());
	size_t bytesSent = send(clientFd, request, sizeof(request), 0);
	printf("[Client %d] Sent msg of size %lu to server\n", tid, bytesSent);
	
	while (true) {
		readBytes = recv(clientFd, response, 1024 - 1, 0);
		if (readBytes > 0 ) {
			if (readBytes > RPC_HEADER_SIZE) {
				Raft::RPCHeader header(response);
				if (readBytes == RPC_HEADER_SIZE + header.payloadLength) {
					Raft::RPC_StateMachineCmd_Request rpcResponse;
					rpcResponse.ParseFromArray(response + RPC_HEADER_SIZE, header.payloadLength);
					printf("[Client %d] received: %s\n", tid, rpcResponse.cmd().c_str());
				}
			}
		} else if (readBytes == 0) {
			close(clientFd);
			printf("[Client %d] received no bytes from server\n", tid);
			break;
		} else {
			printf("[Client %d] Received bad value from server\n", tid);
			perror("Bad Socket");
			close(clientFd);
			break;
		}
	}
	// closing the connected socket
	close(clientFd);
	return;
}

int main(int argc, char const* argv[]) {
	std::vector<std::thread> threads(NUM_THREADS);
	for (int tid = 0; tid < NUM_THREADS; tid++) {
		threads[tid] = std::thread(connectAndSendToServer, tid);
	}

	for (int tid = 0; tid < NUM_THREADS; tid++) {
		threads[tid].join();
	}
	return 0;
}
