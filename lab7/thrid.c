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
#define MAX 16



typedef struct {
    int socket;
    char sendBuffer[BUFFER_SIZE];
    unsigned long long sendLength;
    unsigned long long sendOffset;
} Client;

Client clients[MAX] = {0};


void addClient(int newSocket, struct sockaddr_in *clientAddr) {
    for (int i = 0; i < MAX; i++) {
        if (clients[i].socket == 0) {
            clients[i].socket = newSocket;
            clients[i].sendLength = 0;
            clients[i].sendOffset = 0;
            printf("Client added %s:%d\n", inet_ntoa(clientAddr->sin_addr), ntohs(clientAddr->sin_port));
            return;
        }
    }
    send(newSocket, "Server is full.\n", 17, 0);
    close(newSocket);
}


void deleteClient(int sd) {
    for (int i = 0; i < MAX; i++) {
        if (clients[i].socket == sd) {
            close(clients[i].socket);
            clients[i].socket = 0;
            clients[i].sendLength = 0;
            clients[i].sendOffset = 0;
            break;
        }
    }
}

void flushClientBuffer(Client *client) {
    while (client->sendLength > 0) {
        ssize_t sent = send(client->socket, client->sendBuffer + client->sendOffset, client->sendLength, 0);
        if (sent < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) return;
            perror("Send error");
            deleteClient(client->socket);
            return;
        }
        client->sendOffset += sent;
        client->sendLength -= sent;
        if (client->sendLength == 0) {
            client->sendOffset = 0;
        }
    }
}


void handleMessage(Client *client) {
    char buffer[BUFFER_SIZE];
    ssize_t toRead = read(client->socket, buffer, sizeof(buffer));
    if (toRead <= 0) {
        if (toRead == 0) {
            struct sockaddr_in addr;
            socklen_t len = sizeof(addr);
            getpeername(client->socket, (struct sockaddr *)&addr, &len);
            printf("Client disconnected: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        } else {
            perror("Read error");
        }
        deleteClient(client->socket);
    } else {
        if (toRead < BUFFER_SIZE) buffer[toRead] = '\0';
        if (client->sendLength == 0) {
            ssize_t sent = send(client->socket, buffer, toRead, 0);
            if (sent < toRead) {
                if (sent < 0) sent = 0;
                memcpy(client->sendBuffer, buffer + sent, toRead - sent);
                client->sendLength = toRead - sent;
                client->sendOffset = 0;
            }
        } else {
            if (client->sendLength + toRead < BUFFER_SIZE) {
                memcpy(client->sendBuffer + client->sendLength, buffer, toRead);
                client->sendLength += toRead;
            } else {
                fprintf(stderr, "Send buffer overflow\n");
                deleteClient(client->socket);
            }
        }
    }
}

int setupServer(int port) {
    struct sockaddr_in serverAddr;
    int opt = 1;
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0 ) {
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

    if (listen(serverSocket, 66) < 0) {
        perror("Listen fail");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);
    return serverSocket;
}

void serverLoop(int serverSocket) {
    fd_set readSet, writeSet;
    int maxSocket;
    while (1) {
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        FD_SET(serverSocket, &readSet);
        maxSocket = serverSocket;

        for (int i = 0; i < MAX; i++) {
            int socket = clients[i].socket;
            if (socket > 0) {
                FD_SET(socket, &readSet);
                if (clients[i].sendLength > 0) {
                    FD_SET(socket, &writeSet);
                }
                if (socket > maxSocket) {
                    maxSocket = socket;
                }
            }
        }

        int activity = select(maxSocket + 1, &readSet, &writeSet, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("select error");
            continue;
        }

        if (FD_ISSET(serverSocket, &readSet)) {
            struct sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            int newSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrLen);
            if (newSocket >= 0) {
                addClient(newSocket, &clientAddr);
            } else {
                perror("accept error");
            }
        }

        for (int i = 0; i < MAX; i++) {
            int socket = clients[i].socket;
            if (socket > 0 && FD_ISSET(socket, &readSet)) {
                handleMessage(&clients[i]);
            }
            if (socket > 0 && FD_ISSET(socket, &writeSet)) {
                flushClientBuffer(&clients[i]);
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
    int serverSocket = setupServer(port);
    serverLoop(serverSocket);

    for (int i = 0; i < MAX; i++) {
        if (clients[i].socket > 0) {
            close(clients[i].socket);
        }
    }
    close(serverSocket);
    exit(EXIT_SUCCESS);
}
