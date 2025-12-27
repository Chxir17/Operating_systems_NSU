#ifndef OS_MESSAGES_H
#define OS_MESSAGES_H

#include "../../request/request.h"

void request_init(Request**);
void request_destroy(Request*);
void request_print(Request*);

void parse_method(Request* result, const char* line);
void parse_metadata(Request *result, const char *line);
int request_send(int socket, Request *request);

#endif //OS_MESSAGES_H