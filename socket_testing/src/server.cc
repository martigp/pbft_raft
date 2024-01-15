#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include<sys/poll.h>
#include <unistd.h>
#include <netinet/tcp.h>

#define PORT 1234
#define MAX_CONNECTIONS 1

int createServerSocket(void) {

    int socketFd;
    struct sockaddr_in address;
    int opt = 1;

    // Creating socket file descriptor
    if ((socketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

 
    // Forcefully attaching socket to the port 8080
    if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
 
    // Forcefully attaching socket to the port PORT
    if (bind(socketFd, (struct sockaddr*)&address,
             sizeof(address))
        < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    return socketFd;
}

void acceptNewConnection(int listenSocket,
                         struct pollfd ** pollFds,
                         int *pollCount) {
    // struct sockaddr clientAddr;
    // socklen_t addrLen;
    printf("<1>");
    int clientSocket = accept(listenSocket, NULL, NULL);
    printf("<2>");
    if (clientSocket < 0) {
        printf("[Server] invalid client socket %d generated from listenSocket %d", clientSocket, listenSocket);
        perror("accept");
        exit(EXIT_FAILURE);
    }
    printf("<3>");
    if (*pollCount > MAX_CONNECTIONS) {
        printf("[Server] Can't accept because too many connections\n");
        return;
    }
    printf("<4>");
    (*pollFds)[*pollCount].fd = clientSocket;
    printf("<5>");
    (*pollFds)[*pollCount].events = POLLIN;
    printf("<6>");
    (*pollCount)++;
}

int main(int argc, char const* argv[])
{    
    int listenSocket;

    struct pollfd pollFds[MAX_CONNECTIONS + 1]; // Array of socket file descriptors
    int pollCount; // Current number of descriptors in array

    listenSocket = createServerSocket();
    
    if (listen(listenSocket, MAX_CONNECTIONS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Listening on port %d\n", PORT);

    memset(pollFds, 0, sizeof(pollFds));
    pollFds[0].fd = listenSocket;
    pollFds[0].events = POLLIN;
    pollCount = 1;

    printf("[Server] Set up poll fd array\n");

    int numClientsServiced = 0;

    while (1) {
        int pollStatus = poll(pollFds, pollCount, 2000);

        if (pollStatus == -1) {
            perror("poll");
            exit(EXIT_FAILURE);
        }
        else if (pollStatus == 0) {
            /* No sockets are ready to accepts*/
            printf("[Server] Waiting...\n");
            continue;
        }
        printf("[Server] Poll status %d is non-zero\n", pollStatus);

        for (int i = 0; i < pollCount; i++) {
            if((pollFds[i].revents & POLLIN) != 1) {
                printf("[Server] Different event on socket not ready\n");
                // Socket isn't ready for reading
                continue;
            }

            printf("Client [%d] Ready for I/O operation\n", ++numClientsServiced);
            if (i == 0) {
                /* 
                    Client connected to the server socket, accept connection
                    and add to pollFd to listen for further messages.
                */
                int clientSocket = accept(listenSocket, NULL, NULL);
                if (clientSocket < 0) {
                    printf("[Server] invalid client socket %d generated from listenSocket %d", clientSocket, listenSocket);
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                if (pollCount > MAX_CONNECTIONS) {
                    printf("[Server] Can't accept because too many connections\n");
                    close(clientSocket);
                    continue;
                }
                pollFds[pollCount].fd = clientSocket;
                pollFds[pollCount].events = POLLIN;
                pollCount++;
            }
            else {
                // Read message from client and send data
                ssize_t bytesRead;
                char clientReadBuffer[256];
                char msgToSend[256];
                int clientSocket = pollFds[i].fd;

                memset(clientReadBuffer, 0, sizeof(clientReadBuffer));
                bytesRead = recv(clientSocket, clientReadBuffer, sizeof(clientReadBuffer) - 1, 0);
                if (bytesRead <= 0) {
                    // Error in read
                    printf("Client [%d] Socket closed with because read %s\n", i, bytesRead == 0 ? "client closed socket" : "Error in read");
                    close(clientSocket);
                    pollFds[i] = pollFds[pollCount -1];
                    memset(pollFds + pollCount -1, 0, sizeof(struct pollfd));
                    printf("[Server] Total clients serviced: %d\n", numClientsServiced);
                    pollCount--;
                }
                else {
                    // Print out client message and send confirmation
                    printf("[Server] Client %d sent message: %s\n", clientSocket, clientReadBuffer);
                    memset(msgToSend, 0, sizeof(msgToSend));
                    snprintf(msgToSend, sizeof(msgToSend), "Received your message: %s", clientReadBuffer);
                    if (send(clientSocket, msgToSend, strnlen(msgToSend, sizeof(msgToSend)), 0) == -1) {
                        perror("Send to client");
                        exit(EXIT_FAILURE);
                    }

                }
            }
        }


    }
}