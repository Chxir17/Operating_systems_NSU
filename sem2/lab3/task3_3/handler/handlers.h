#ifndef OS_HANDLERS_H
#define OS_HANDLERS_H
#include <sys/types.h>
#include "../proxy/proxy.h"

void init_server_socket(int *server_socket, int port, int max_clients);
Request *read_header(int socket);
int http_connect(Request *req);
char *read_body(int socket, ssize_t *length, int max_buffer);
int send_to_client(int client_socket, char data[], int packages_size, long length);
char *read_line(int socket, long *length);

#endif //OS_HANDLERS_H