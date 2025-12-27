#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <time.h>

#include "handlers.h"
#include "messages/messages.h"

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
	Request *requests;
	request_init(&requests);
    long buffer_length;
	char *buffer = read_line(socket, &buffer_length);

    printf("Header: %s\n", buffer);

	parse_method(requests, buffer);

	while(1) {
		buffer = read_line(socket, &buffer_length);
		if(buffer[0] == '\r' && buffer[1] == '\n') {
			break;
		}

	    //заголоки
		parse_metadata(requests, buffer);
		free(buffer);
	}
	return requests;
}

char *read_body(int socket, long *length, int max_buffer) {
    if (length == NULL) {
        printf("The length pointer supplied to read_chunk is NULL\n");
        return NULL;
    }

    if(socket == -1) {
        printf("The socket given to read_chunk is invalid\n");
        return NULL;
    }

    char *buf = malloc(max_buffer + 1);
    if (buf == NULL) {
        perror("Malloc in read_body");
        return NULL;
    }

	memset(buf, '\0', max_buffer + 1);
	long total_bytes = 0;
    long num_bytes = 0;

	while (total_bytes < max_buffer) {
		num_bytes = recv(socket, buf + total_bytes, max_buffer - total_bytes, 0);
        if (num_bytes == -1) {
            perror("recv in http_read_body");
            free(buf);
            return NULL;
        }
        if (num_bytes == 0) {
            break;
        }

        total_bytes += num_bytes;
	}

    if (total_bytes == 0) {
        printf("The end get body from website\n");

        return NULL;
    }
	*length = total_bytes;
	return buf;
}

int http_connect(Request *req) {
	char *host = (char*)list_get_key(&req->metadata_head, "Host");
    char *port = strstr(host, ":");

    if(port == NULL) {
        port = (char*)calloc(3, sizeof(char) + 1);
        strcat(port, "80");
    }
    else {
        host = strtok(host, ":");
        port++;
    }
    printf("Connecting to HTTP server: %s\n", host);
	if(host == NULL) {
		return -1;
	}

    int website_socket;
    struct hostent *host_info;
    struct sockaddr_in server_address;

    if ((website_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        return -1;
    }

    if ((host_info = gethostbyname(host)) == NULL) {
        perror("Error getting host by name");
        close(website_socket);
        return -1;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    memcpy(&server_address.sin_addr.s_addr, host_info->h_addr_list[0], host_info->h_length);
    server_address.sin_port = htons(atoi(port));

    if (connect(website_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error connecting to server");
        close(website_socket);
        return -1;
    }

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

char *read_line(int socket, long *length) {
    int buffer_size = 3;
    char *buffer = malloc(sizeof(char) * buffer_size + 1);
    if (!buffer) {
        perror("Malloc in read_line()");
        abort();
    }
    char c;
    long recbytes = 0;
    int counter = 0;

    while(1) {
        recbytes = recv(socket, &c, 1, 0);
        if (recbytes == -1) {
            perror("Recv");
        } else if (recbytes == 0) {
            buffer[0] = '\r';
            buffer[1] = '\n';
            buffer[2] = '\0';

            *length = 3;
            return buffer;
        }
        buffer[counter++] = c;

        if (c == '\n') {
            buffer[counter] = '\0';

            *length = counter;
            return buffer;
        }

        if (counter >= buffer_size) {
            buffer_size += 1;
            char *tmp = realloc(buffer, sizeof(char) * buffer_size + 1);
            if (!tmp) {
                perror("realloc in read_line()");
                free(buffer);
                abort();
            }
            buffer = tmp;
        }
    }
}