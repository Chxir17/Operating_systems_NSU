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

// list.h
typedef struct List {
    Node* first;
    Node* last;
    pthread_mutex_t mutex;      // для синхронизации читателей
    pthread_cond_t cond;        // пробуждение при новом чанке
    int complete;               // завершена ли загрузка
} List;

typedef struct Cache {
    char* url;
    List* response;
    struct Cache* next;
    int is_complete;
    int loading;
    pthread_cond_t cond;
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
#endif //OS_LIST_H