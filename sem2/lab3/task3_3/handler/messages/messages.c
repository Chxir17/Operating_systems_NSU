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
    if (!req) {
        return;
    }
    free(req->search_path);
    Metadata_item *item = TAILQ_FIRST(&req->metadata_head);
    while (item) {
        Metadata_item *tmp = TAILQ_NEXT(item, entries);
        TAILQ_REMOVE(&req->metadata_head, item, entries);
        free((void*)item->key);
        free((void*)item->value);
        free(item);
        item = tmp;
    }
    free(req);
}
void update_request_from_location(Request *req, const char *location) {
    char host[256] = {0};
    char path[2048] = "/";

    if (sscanf(location, "http://%255[^/]%2047s", host, path) >= 1) {
        request_set_host(req, host);
        request_set_url(req, location);
    }
}

void request_set_url(Request *req, const char *new_url) {
    free(req->search_path);
    req->search_path = strdup(new_url);
}
void request_set_host(Request *req, const char *host) {
    Metadata_item *item;
    TAILQ_FOREACH(item, &req->metadata_head, entries) {
        if (strcasecmp(item->key, "Host") == 0) {
            free((void*)item->value);
            item->value = strdup(host);
            return;
        }
    }

    item = malloc(sizeof(*item));
    item->key = strdup("Host");
    item->value = strdup(host);
    TAILQ_INSERT_TAIL(&req->metadata_head, item, entries);
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

    char* pointer;
    int len;
    char *copy = pointer = strdup(buffer);
    const char* token = NULL;
    int state = METHOD;

    //бьем 1 строку на части
    /*Например, из "GET /path HTTP/1.1\r\n" получим токены:
    "GET"
    "/path"
    "HTTP/1.1"
    */
    while ((token = strsep(&pointer, " \r\n")) != NULL) {
        if (state == METHOD) {
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
            state++;

        } else if (state == URL) {
            len = strlen(token);
            result->search_path = strndup(token, len);
            state++;

        } else if (state == VERSION) {
            if (strncmp(token, "HTTP/1.0", strlen("HTTP/1.0")) == 0) {
                result->version = HTTP_VERSION_1_0;
            } else if (strncmp(token, "HTTP/1.1", strlen("HTTP/1.1")) == 0) {
                result->version = HTTP_VERSION_1_1;
            } else {
                result->version = HTTP_VERSION_INVALID;
            }
            state++;

        } else if (state == DONE) {
            break;
        }
    }

    free(copy);
}

void parse_metadata(Request *result, const char *buffer) {
    char *buffer_copy = strdup(buffer);
    char *save_pointer = NULL;
    char *key_token = strtok_r(buffer_copy, ":", &save_pointer);
    if (!key_token) {
        free(buffer_copy);
        return;
    }
    char *value_token = strtok_r(NULL, "\r", &save_pointer);
    if (!value_token) {
        free(buffer_copy);
        return;
    }
    while (*value_token == ' ') {
        value_token++;
    }
    Metadata_item *item = malloc(sizeof(*item));
    item->key = strdup(key_token);
    item->value = strdup(value_token);
    TAILQ_INSERT_TAIL(&result->metadata_head, item, entries);
    free(buffer_copy);
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

char *build_request(Request *req, long *length) {
    const char *search_path = req->search_path;
    char* uri = extract_uri(search_path);
    long size = strlen("GET ") + strlen(uri);

    if (req->version == HTTP_VERSION_1_0) {
        size += strlen(" HTTP/1.0\r\n");
    } else if (req->version == HTTP_VERSION_1_1) {
        size += strlen(" HTTP/1.1\r\n");
    } else {
        free(uri);
        return NULL;
    }

    Metadata_item *item;
    TAILQ_FOREACH(item, &req->metadata_head, entries) {
        if (strcmp(item->key, "Connection") == 0 || strcmp(item->key, "Proxy-Connection") == 0) {
            continue;
        }
        size += strlen(item->key) + 2 + strlen(item->value) + 2; // key + ": " + value + "\r\n"
    }

    if (req->version == HTTP_VERSION_1_1) {
        size += strlen("Connection: close\r\n");
    }

    size += 2; //"\r\n"

    char* request_buffer = malloc(size + 1);
    if (!request_buffer) {
        perror("malloc in build_request");
        free(uri);
        return NULL;
    }

    strcpy(request_buffer, "GET ");
    strcat(request_buffer, uri);
    free(uri);

    if (req->version == HTTP_VERSION_1_0) {
        strcat(request_buffer, " HTTP/1.0\r\n");
    } else {
        strcat(request_buffer, " HTTP/1.1\r\n");
    }

    TAILQ_FOREACH(item, &req->metadata_head, entries) {
        if (strcmp(item->key, "Connection") == 0 || strcmp(item->key, "Proxy-Connection") == 0) {
            continue;
        }
        strcat(request_buffer, item->key);
        strcat(request_buffer, ": ");
        strcat(request_buffer, item->value);
        strcat(request_buffer, "\r\n");
    }

    if (req->version == HTTP_VERSION_1_1) {
        strcat(request_buffer, "Connection: close\r\n");
    }

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