#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 16

int serverSocket;
int clientSockets[MAX_CLIENTS] = {0};


void sigintHandler(int sig) {
    printf("\nServer down\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientSockets[i] > 0) {
            send(clientSockets[i], "Server down.\n", 26, 0);
            close(clientSockets[i]);
        }
    }
    close(serverSocket);
    exit(EXIT_SUCCESS);
}

void removeClient(int sd) {
    close(sd);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientSockets[i] == sd) {
            clientSockets[i] = 0;
            break;
        }
    }
}

void handleClientMessage(int sd) {
    char buffer[BUFFER_SIZE];
    ssize_t valread = read(sd, buffer, sizeof(buffer));
    if (valread < 0) {
        perror("Read error");
        removeClient(sd);
    }
    else if (valread == 0) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        getpeername(sd, (struct sockaddr *)&addr, &len);
        printf("Client disconnected: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        removeClient(sd);
    }
    else {
        if (valread < BUFFER_SIZE) buffer[valread] = '\0';
        if (send(sd, buffer, valread, 0) != valread) {
            perror("Send error");
            removeClient(sd);
        }
    }
}

void addNewClient(int newSocket, struct sockaddr_in *clientAddr) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientSockets[i] == 0) {
            clientSockets[i] = newSocket;
            printf("Client added %s:%d at slot %d\n", inet_ntoa(clientAddr->sin_addr), ntohs(clientAddr->sin_port), i);
            return;
        }
    }

    printf("Max clients. Reject %s:%d\n", inet_ntoa(clientAddr->sin_addr), ntohs(clientAddr->sin_port));
    send(newSocket, "Server is full.\n", 33, 0);
    close(newSocket);
}

void setupServer(int port) {
    struct sockaddr_in serverAddr;
    int opt = 1;
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket create fail");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind fail");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 10) < 0) {
        perror("Listen fail");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);
}

void serverLoop() {
    fd_set readFds;
    int maxSd;
    while (1) {
        FD_ZERO(&readFds);
        FD_SET(serverSocket, &readFds);
        maxSd = serverSocket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clientSockets[i];
            if (sd > 0) {
                FD_SET(sd, &readFds);
            }
            if (sd > maxSd){
                maxSd = sd;
            }
        }

        int activity = select(maxSd + 1, &readFds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("select error");
            continue;
        }

        if (FD_ISSET(serverSocket, &readFds)) {
            struct sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            int newSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrLen);
            if (newSocket < 0) {
                perror("accept error");
            }
            else {
                printf("New connection: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
                addNewClient(newSocket, &clientAddr);
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clientSockets[i];
            if (sd > 0 && FD_ISSET(sd, &readFds)) {
                handleClientMessage(sd);
            }
        }
    }
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int port = atoi(argv[1]);
    signal(SIGINT, sigintHandler);
    setupServer(port);
    serverLoop();

    printf("\nServer down\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientSockets[i] > 0) {
            send(clientSockets[i], "Server down.\n", 26, 0);
            close(clientSockets[i]);
        }
    }
    close(serverSocket);
    exit(EXIT_SUCCESS);
}
