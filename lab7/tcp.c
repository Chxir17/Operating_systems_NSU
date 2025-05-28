#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define BACKLOG 10

void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        if (send(clientSocket, buffer, bytesRead, 0) == -1) {
            perror("Send error");
            break;
        }
    }
    if (bytesRead == -1) {
        perror("Receive error");
    }
    close(clientSocket);
    exit(EXIT_SUCCESS);
}

void sigchldHandler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void setupSigchldHandler() {
    struct sigaction sa = {
        .sa_handler = sigchldHandler,
        .sa_flags = SA_RESTART
    };
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Sigaction error");
        exit(EXIT_FAILURE);
    }
}

int setupServerSocket(const int port) {
    int sockfd;
    struct sockaddr_in serverAddr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket create fail");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind fail");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    if (listen(sockfd, BACKLOG) < 0) {
        perror("Listen fail");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int port = atoi(argv[1]);
    int serverSocket = setupServerSocket(port);
    setupSigchldHandler();
    printf("TCP-server listening on port %d.\n", port);
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("New connection from %s:%d\n",
               inet_ntoa(clientAddr.sin_addr),
               ntohs(clientAddr.sin_port));

        pid_t pid = fork();
        if (pid == 0) {
            close(serverSocket);
            handleClient(clientSocket);
        }
        else if (pid < 0) {
            perror("Fork failed");
        }
        close(clientSocket);
    }
    close(serverSocket);
    return 0;
}
