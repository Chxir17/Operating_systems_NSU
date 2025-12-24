#ifndef OS_PROXY_H
#define OS_PROXY_H
#include <sys/queue.h>

enum Methods_enum {
    OPTIONS,
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    TRACE,
    CONNECT,
    UNKNOWN
};

enum Versions_enum {
    HTTP_VERSION_1_0,
    HTTP_VERSION_1_1,
    HTTP_VERSION_INVALID
};

typedef struct Request
{
    enum Methods_enum method;
    enum Versions_enum version;
    char *search_path;
    //двусвязный список с хвостом для добавления элементов
    TAILQ_HEAD(METADATA_HEAD, Metadata_item) metadata_head;
} Request;

typedef struct Metadata_item {
    const char *key;
    const char *value;
    TAILQ_ENTRY(Metadata_item) entries;
} Metadata_item;

char *read_line(int socket, long *length);
const char *list_get_key(struct METADATA_HEAD *list, const char *key);
void list_add_key(struct METADATA_HEAD *list, const char *key, const char *value);
#endif //OS_PROXY_H