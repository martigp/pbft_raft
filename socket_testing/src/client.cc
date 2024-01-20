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

#define PORT 1234
#define NUM_THREADS 1

std::mutex globalLock;
int numServiced = 0;

void connectAndSendToServer(int tid)
{
restart:
	int status, readBytes, clientFd;
	struct sockaddr_in serv_addr;
	const char* hello = "Message sent from client";
	char buffer[1024] = { 0 };
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
		return;
	}
	printf("[Client %d] Connected to server\n", tid);

	send(clientFd, hello, strlen(hello), 0);
	printf("[Client %d] Sent msg %s to server\n", tid, hello);
	while (true) {
		readBytes = read(clientFd, buffer, 1024 - 1);
		if (readBytes > 0 ) {
			printf("[Client %d] received: %s\n", tid, buffer);
			globalLock.lock();
			printf("Number served %d", ++numServiced);
			globalLock.unlock();

		} else if (readBytes == 0) {
			printf("[Client %d] received no bytes from server\n", tid);
			break;
		} else {
			printf("[Client %d] Received bad value from server\n", tid);
			close(clientFd);
			goto restart;
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
	printf("Total serviced: %d", numServiced);
	return 0;
}
