#ifndef OS_HANDLERS_H
#define OS_HANDLERS_H
#include <sys/types.h>
#include "../request/request.h"

void init_server_socket(int *server_socket, int port, int max_clients);
Request *read_header(int socket);
int http_connect(Request *req);
long read_body(int socket, char *buf, long max_buffer);
int send_to_client(int client_socket, char data[], int packages_size, long length);
long read_line(int socket, char *buf, long maxlen);
int parse_http_status(const char *status_line);
int is_redirect(int status);
char *get_location_header(Request *req);
#endif //OS_HANDLERS_H