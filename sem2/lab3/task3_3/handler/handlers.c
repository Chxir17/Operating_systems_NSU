#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <time.h>

#include "handlers.h"
#include "messages/messages.h"

#define LINE_BUFFER_SIZE 8192

void init_server_socket(int *server_socket, int port, const int max_clients) {
    //дескриптор сокета
    //IPV4, TCP, протокол автоматически
    *server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_socket == -1) {
        perror("Can't create server socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    //дескриптор, общие параметры сокета, повторно использовать локальный адрес, включить
    //дескриптор, на каком уровне, опция, вкл/выкл, размер опции
    if (setsockopt(*server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Can't set socket port");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; //ipv4
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);//все интерфейсы htonl(0.0.0.0) в сетевой порядок (от старшего байта к младшему)
    server_addr.sin_port = htons(port);

    //привязка сокета к адресу
    if (bind(*server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        close(*server_socket);
        exit(EXIT_FAILURE);
    }

    //слушающий сокет
    if (listen(*server_socket, max_clients) == -1) {
        perror("Error listening");
        close(*server_socket);
        exit(EXIT_FAILURE);
    }
}

Request *read_header(const int socket) {
    Request *request;
    request_init(&request);

    char buffer[LINE_BUFFER_SIZE];

    if (read_line(socket, buffer, sizeof(buffer)) <= 0) {
        free(request);
        return NULL;
    }

    printf("Header: %s\n", buffer);

    parse_method(request, buffer);
    while (1) {
        const long len = read_line(socket, buffer, sizeof(buffer));
        if (len <= 0) {
            break;
        }
        if (strcmp(buffer, "\r\n") == 0) {
            break;
        }

        //заголоки
        parse_metadata(request, buffer);
    }
    return request;
}

long read_body(int socket, char *buf, long max_buffer) {
    if(socket == -1) {
        printf("The socket given to read_chunk is invalid\n");
        return 0;
    }
    long total = 0;
    while (total < max_buffer) {
        long num_bytes = recv(socket, buf + total, max_buffer - total, 0);
        if (num_bytes <= 0) {
            break;
        }
        total += num_bytes;
    }
    return total > 0 ? total : -1;
}

int http_connect(Request *req) {
    const char *host_header = list_get_key(&req->metadata_head, "Host");
    if (!host_header) {
        fprintf(stderr, "No Host header\n");
        return -1;
    }

    char host[256];
    char port_str[6] = "80";

    strncpy(host, host_header, sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';

    char *colon_pointer = strchr(host, ':');
    if (colon_pointer) {
        *colon_pointer = '\0';
        strncpy(port_str, colon_pointer + 1, sizeof(port_str) - 1);
        port_str[sizeof(port_str) - 1] = '\0';
    }

    printf("Connecting to HTTP server: %s:%s\n", host, port_str);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0 || !res) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    int website_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (website_socket == -1) {
        perror("Error creating socket");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(website_socket, res->ai_addr, res->ai_addrlen) < 0) {
        perror("Error connecting to server");
        close(website_socket);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    printf("Successfully connected to HTTP server: %s\n", host);

    return website_socket;
}

int send_to_client(int client_socket, char* data, int packages_size, long length) {
    if (length <= 0) {
        return 0;
    }

    int bytes_sent = -1;
    if (packages_size <= 0) {
        bytes_sent = send(client_socket, data, length, 0);
    } else {
        int p;
        for (p = 0; p * packages_size + packages_size < length; p++) {
            bytes_sent = send(client_socket, (data + p * packages_size), packages_size, 0);
            if (bytes_sent == -1) {
                perror("Couldn't send data to the client (loop)");
                return -1;
            }
        }

        if (p * packages_size < length) {
            bytes_sent = send(client_socket, (data + p * packages_size), length - p * packages_size, 0);
        }
    }

    if (bytes_sent == -1) {
        perror("Couldn't send data to the client");
        return -1;
    }

    return 0;
}

long read_line(int socket, char *buf, long max_len) {
    long len = 0;
    char read;
    while (len + 1 < max_len) {
        const long read_bytes = recv(socket, &read, 1, 0);
        if (read_bytes <= 0) {
            break;
        }
        buf[len++] = read;
        if (read == '\n') {
            break;
        }
    }
    buf[len] = '\0';
    return len;
}