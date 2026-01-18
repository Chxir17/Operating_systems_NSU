#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

int createUdpSocket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0){
        perror("Socket create failed");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

struct hostent *resolveHostname(const char *serverIP) {
    struct hostent *server = gethostbyname(serverIP);///йetc/hosts
    if (server == NULL) {
        fprintf(stderr, "Host %s not found\n", serverIP);
        exit(EXIT_FAILURE);
    }
    return server;
}

void prepareServerAddress(struct sockaddr_in *addr, struct hostent *server, int port) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    memcpy(&addr->sin_addr.s_addr, server->h_addr, server->h_length);
    addr->sin_port = htons(port);
}

void sendMessage(int sockfd, const char *message, const struct sockaddr_in *serverAddr) {
    ssize_t msgLen = sendto(sockfd, message, strlen(message), 0, (const struct sockaddr *) serverAddr, sizeof(*serverAddr));
    if (msgLen < 0){
        perror("sendto failed");
        exit(EXIT_FAILURE);
    }
}

void receiveReply(int sockfd, struct sockaddr_in *serverAddr) {
    char buffer[BUFFER_SIZE];
    socklen_t addrLen = sizeof(*serverAddr);
    ssize_t msgLen = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *) serverAddr, &addrLen);
    if (msgLen < 0){
        perror("recvfrom failed");
        exit(EXIT_FAILURE);
    }
    buffer[msgLen] = '\0';
    printf("Reply: %s\n", buffer);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server ip> <port> <message>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *serverIP = argv[1];
    int port = atoi(argv[2]);
    const char *message = argv[3];
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %d\n", port);
        return EXIT_FAILURE;
    }

    int sockfd = createUdpSocket();
    struct hostent *server = resolveHostname(serverIP);
    struct sockaddr_in serverAddr;
    prepareServerAddress(&serverAddr, server, port);
    sendMessage(sockfd, message, &serverAddr);
    receiveReply(sockfd, &serverAddr);
    close(sockfd);
    return 0;
}
