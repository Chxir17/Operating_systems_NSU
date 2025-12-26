#ifndef OS_LIST_H
#define OS_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct Node {
    char* value;
    long size;
    struct Node* next;
    pthread_rwlock_t sync;
    pthread_cond_t cond;      // оповещение для новых данных
    int ready;                // флаг, что данные готовы для чтения
} Node;

typedef struct List {
    Node* first;
    Node* last;
    pthread_mutex_t mutex;    // блокировка добавления новых узлов
} List;

typedef enum {
    CACHE_LOADING,
    CACHE_READY,
    CACHE_ERROR
} cache_state_t;

typedef struct Cache {
    char* url;
    List* response;

    cache_state_t state;

    pthread_mutex_t mutex;
    pthread_cond_t  cond;

    struct Cache* next;
} Cache;


typedef struct Map {
    Cache* first;
    pthread_mutex_t mutex;
} Map;

Map* map_init();
Cache* map_add(Map* map, const char* url);
Cache* map_find_by_url(const Map* map, const char* url);

List* list_init();
Node* list_add(List* list, const char* value, long length);
void list_destroy(List* list);

#endif //OS_LIST_H
