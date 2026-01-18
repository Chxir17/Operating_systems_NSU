#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024

int createSocket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket create fail");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

void connectToServer(int sockfd, const char *ip, uint16_t port) {
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        //конвертация текста в двоичный
        perror("Invalid address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection fail");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
}

void communicate(int sockfd, const char *serverIP, int port) {
    char buffer[BUF_SIZE];
    ssize_t bytesReceived;
    printf("Connected!");
    while (1) {
        printf("Your message: ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("\nInput error or EOF.\n");
            break;
        }
        ssize_t toSend = strlen(buffer);
        if (send(sockfd, buffer, toSend, 0) != toSend) {
            perror("Send fail");
            break;
        }
        bytesReceived = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            perror("Receive fail");
            break;
        }
        buffer[bytesReceived] = '\0';
        printf("reply: %s", buffer);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *serverIP = argv[1];
    int port = atoi(argv[2]);
    int clientSocket = createSocket();
    connectToServer(clientSocket, serverIP, port);
    communicate(clientSocket, serverIP, port);
    close(clientSocket);
    return 0;
}
