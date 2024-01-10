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
#define PORT 1234

int main(int argc, char const* argv[])
{
	int status, valread, clientFd;
	struct sockaddr_in serv_addr;
	char* hello = "Message sent from client";
	char buffer[1024] = { 0 };
	if ((clientFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary
	// form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)
		<= 0) {
		printf(
			"\nInvalid address/ Address not supported \n");
		return -1;
	}

	if ((status
		= connect(clientFd, (struct sockaddr*)&serv_addr,
				sizeof(serv_addr)))
		< 0) {
		printf("\nConnection Failed \n");
		return -1;
	}

	send(clientFd, hello, strlen(hello), 0);
	printf("Hello message sent\n");
	valread = read(clientFd, buffer, 1024 - 1); // subtract 1 for the null terminator at the end
    if (valread < 0) {
        printf("Vald read was %d\n", valread);
    }
    
    printf("Vald read was %d\n", valread);
	printf("%s\n", buffer);

	// closing the connected socket
	close(clientFd);
	return 0;
}
