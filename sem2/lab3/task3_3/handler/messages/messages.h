#ifndef OS_MESSAGES_H
#define OS_MESSAGES_H

#include "../../request/request.h"
#define MAX_REDIRECTS 5

void request_init(Request**);
void request_destroy(Request*);
void request_print(Request*);
void request_set_url(Request *req, const char *new_url);
void request_set_host(Request *req, const char *host);
void update_request_from_location(Request *req, const char *location);
void parse_method(Request* result, const char* line);
void parse_metadata(Request *result, const char *line);
int request_send(int socket, Request *request);

#endif //OS_MESSAGES_H