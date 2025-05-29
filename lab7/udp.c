#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

int createUdpSocket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);// протокол, с которым будет работать сокет ipv4, характер передаваемых данных datagram, протокол
    if (sockfd < 0){
        perror("Socket create fail");
        exit(EXIT_FAILURE);
    }
    int param = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &param, sizeof(param)) < 0){ // уровень сокета, повторное использование адреса и порта
        perror("Setsockopt fail");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

void bindSocketToPort(int sockfd, int port) {
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;//привязка ко всем доступным интерфейсам
    serverAddr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0){
        perror("Bind fail");
        exit(EXIT_FAILURE);
    }
}

void runEchoServer(int sockfd) {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    while (1) {


       ssize_t recvLen = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &clientAddr, &clientLen);//кол-во байт
        if (recvLen < 0){
            perror("recvfrom fail");
            exit(EXIT_FAILURE);
        }
        ssize_t sentLen = sendto(sockfd, buffer, recvLen, 0, (struct sockaddr *) &clientAddr, clientLen);
        if (sentLen < 0){
            perror("sendto fail");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    if (port < 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %d\n", port);
        return EXIT_FAILURE;
    }
    int sockfd = createUdpSocket();
    bindSocketToPort(sockfd, port);
    runEchoServer(sockfd);
    close(sockfd);
    return 0;
}
