#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <malloc.h>

#include "messages.h"
#include "../handlers.h"

int methods_len = 9;
const char *methods[] = {
        "OPTIONS",
        "GET",
        "HEAD",
        "POST",
        "PUT",
        "DELETE",
        "TRACE",
        "CONNECT",
        "INVALID"
};

void request_init(Request **req) {
    *req = malloc(sizeof(Request));
    Request *request = *req;
    request->method = 0;
    request->search_path = NULL;
    TAILQ_INIT(&request->metadata_head);
}

void request_destroy(Request *req) {
    free(req->search_path);
    Metadata_item *item;
    TAILQ_FOREACH(item, &req->metadata_head, entries) {
        free((char*)item->key);
        free((char*)item->value);
        free(item);
    }
}

void request_print(Request *req) {
    printf("HTTP_REQUEST \n");
    if (req->version == HTTP_VERSION_1_0) {
        printf("version:\tHTTP/1.0\n");
    } else if (req->version == HTTP_VERSION_1_1) {
        printf("version:\tHTTP/1.1\n");
    } else {
        printf("version:\tInvalid\n");
    }
    printf("Method:\t\t%s\n",methods[req->method]);
    printf("Path:\t\t%s\n",req->search_path);
    printf("Metadata\n");
    Metadata_item *item;
    TAILQ_FOREACH(item, &req->metadata_head, entries) {
        printf("%s: %s\n", item->key, item->value);
    }
    printf("\n");
}

void parse_method(Request* result, const char* buffer) {
    enum parser_states {
        METHOD,
        URL,
        VERSION,
        DONE
    };

    char* p;
    int len;
    char *copy = p = strdup(buffer);
    const char* token = NULL;
    int s = METHOD;

    while ((token = strsep(&p, " \r\n")) != NULL) {
        if (s == METHOD) {
            int found = 0;
            for (int i = 0; i < methods_len; i++) {
                if (strcmp(token, methods[i]) == 0) {
                    found = 1;
                    result->method = i;
                    break;
                }
            }
            if (!found) {
                result->method = methods_len - 1;
                free(copy);
                return;
            }
            s++;

        } else if (s == URL) {
            len = strlen(token);
            result->search_path = strndup(token, len);
            s++;

        } else if (s == VERSION) {
            if (strncmp(token, "HTTP/1.0", strlen("HTTP/1.0")) == 0) {
                result->version = HTTP_VERSION_1_0;
            } else if (strncmp(token, "HTTP/1.1", strlen("HTTP/1.1")) == 0) {
                result->version = HTTP_VERSION_1_1;
            } else {
                result->version = HTTP_VERSION_INVALID;
            }
            s++;

        } else if (s == DONE) {
            break;
        }
    }

    free(copy);
}

void parse_metadata(Request *result, const char *buffer) {
    char *buffer_copy = strdup(buffer);
    const char *key = strdup(strtok(buffer_copy, ":"));
    char *value = strtok(NULL, "\r");
    char *p = value;
    while (*p == ' ') {
        p++;
    }
    value = strdup(p);
    free(buffer_copy);
    Metadata_item *item = malloc(sizeof(*item));
    item->key = key;
    item->value = value;
    TAILQ_INSERT_TAIL(&result->metadata_head, item, entries);
}

char* extract_uri(const char* search_path) {
    const char* path_start = strstr(search_path, "//");
    char* uri;
    if (path_start) {
        path_start += 2;
        path_start = strchr(path_start, '/');
        if (path_start) {
            long path_length = strlen(path_start);
            uri = (char*)malloc(path_length + 1);
            if (!uri) {
                perror("malloc in parse_uri");
                free(uri);
                exit(EXIT_FAILURE);
            }
            strncpy(uri, path_start, path_length + 1);
        } else {
            uri = strndup("/", 1);
        }
    } else {
        uri = strndup("/", 1);
    }
    return uri;
}

char *build_request(Request *req, ssize_t *length) {
    const char *search_path = req->search_path;
    char* uri = extract_uri(search_path);
    long size = strlen("GET ");
    char *request_buffer = malloc(size + 1);
    strcpy(request_buffer, "GET ");

    size += strlen(search_path) + 1;
    request_buffer = (char*)realloc(request_buffer, size);
    strcat(request_buffer, uri);
    printf("SEARCH PATH: %s\n", request_buffer);

    if (req->version == HTTP_VERSION_1_0) {
        size += strlen(" HTTP/1.0\r\n");
        request_buffer = (char*)realloc(request_buffer, size);
        strcat(request_buffer, " HTTP/1.0\r\n");

    } else if (req->version == HTTP_VERSION_1_1) {
        size += strlen(" HTTP/1.1\r\n");
        request_buffer = (char*)realloc(request_buffer, size);
        strcat(request_buffer, " HTTP/1.1\r\n");

    } else {
        free(request_buffer);
        return NULL;
    }

    Metadata_item *item;
    TAILQ_FOREACH(item, &req->metadata_head, entries) {
        if (strcmp(item->key, "Connection") == 0 || strcmp(item->key, "Proxy-Connection") == 0) {
            continue;
        }

        size += strlen(item->key) + strlen(": ") + strlen(item->value) + strlen("\r\n");
        request_buffer = (char*)realloc(request_buffer, size);
        strcat(request_buffer, item->key);
        strcat(request_buffer, ": ");
        strcat(request_buffer, item->value);
        strcat(request_buffer, "\r\n");
    }

    if (req->version == HTTP_VERSION_1_1) {
        size += strlen("Connection: close\r\n");
        request_buffer = (char*)realloc(request_buffer, size);
        strcat(request_buffer, "Connection: close\r\n");
    }

    size += strlen("\r\n");
    request_buffer = (char*)realloc(request_buffer, size);
    strcat(request_buffer, "\r\n");

    *length = size;
    return request_buffer;
}

int request_send(int socket, Request *request) {
    long length;
    char *request_buffer = build_request(request, &length);
    if (request_buffer == NULL) {
        printf("Message is null\n");
        return -1;
    }
    int err = send_to_client(socket, request_buffer, 0, length);
    if (err == -1) {
        free(request_buffer);
        perror("Send");
        return -1;
    }
    free(request_buffer);
    printf("Sent HTTP header to web server\n");

    return 0;
}