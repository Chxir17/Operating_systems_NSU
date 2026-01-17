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
} Node;

typedef struct List {
    Node* first;
    Node* last;
    pthread_mutex_t mutex;      // для читателей
    pthread_cond_t cond;        // пробуждение при новом части
    int complete;
} List;

typedef struct Cache {
    char* url;
    List* response;
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
void list_print(const List* list);
void list_destroy(List* list);
int list_wait_for_data(List* list, Node** current);
void map_remove(Map* map, const char* url);
#endif //OS_LIST_H