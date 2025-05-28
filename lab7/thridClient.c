#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define BUF_SIZE 1024

int clientSocket;

void sigintHandler(int sig) {
    printf("\nClient down...\n");
    close(clientSocket);
    exit(EXIT_SUCCESS);
}

void connectToServer(struct sockaddr_in *serverAddr, const char* serverIP, int port) {
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(serverAddr, 0, sizeof(*serverAddr));
    serverAddr->sin_family = AF_INET;
    serverAddr->sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP, &serverAddr->sin_addr) <= 0) {
        perror("Invalid address");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    if (connect(clientSocket, (struct sockaddr *)serverAddr, sizeof(*serverAddr)) < 0) {
        perror("Connection failed");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }
    printf("Connected to server %s:%d\n", serverIP, port);
}

void clientLoop() {
    char buffer[BUF_SIZE];
    while (1) {
        printf("Your message: ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("Input error or EOF.\n");
            break;
        }

        size_t msgLen = strlen(buffer);
        if (send(clientSocket, buffer, msgLen, 0) != (ssize_t)msgLen) {
            perror("Send failed");
            break;
        }

        ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived < 0) {
            perror("Receive error");
            break;
        } else if (bytesReceived == 0) {
            printf("Server closed the connection.\n");
            break;
        }

        buffer[bytesReceived] = '\0';
        printf("Server reply: %s", buffer);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *serverIP = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in server_addr;
    signal(SIGINT, sigintHandler);

    connectToServer(&server_addr, serverIP, port);
    clientLoop();

    printf("\nClient down...\n");
    close(clientSocket);
    exit(EXIT_SUCCESS);
}
