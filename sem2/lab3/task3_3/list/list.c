#include "list.h"
#include <string.h>

Map* map_init() {
    Map* map = malloc(sizeof(Map));
    map->first = NULL;
    pthread_mutex_init(&map->mutex, NULL);
    return map;
}

Cache* map_add(Map* map, const char* url) {
    Cache* newCache = malloc(sizeof(Cache));
    newCache->url = strdup(url);
    newCache->response = list_init();

    pthread_mutex_lock(&map->mutex);
    newCache->next = map->first;
    map->first = newCache;
    pthread_mutex_unlock(&map->mutex);
    return newCache;
}

Cache* map_find_by_url(const Map* map, const char* url) {
    pthread_mutex_lock((pthread_mutex_t*)&map->mutex);
    Cache* current = map->first;
    while (current) {
        if (strcmp(current->url, url) == 0) {
            pthread_mutex_unlock((pthread_mutex_t*)&map->mutex);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock((pthread_mutex_t*)&map->mutex);
    return NULL;
}

List* list_init() {
    List* list = malloc(sizeof(List));
    list->first = list->last = NULL;
    pthread_mutex_init(&list->mutex, NULL);
    return list;
}

Node* list_add(List* list, const char* value, long length) {
    Node* node = malloc(sizeof(Node));
    node->value = malloc(length);
    memcpy(node->value, value, length);
    node->size = length;
    node->next = NULL;
    node->ready = 0;

    pthread_rwlock_init(&node->sync, NULL);
    pthread_cond_init(&node->cond, NULL);

    pthread_mutex_lock(&list->mutex);
    if (!list->first) {
        list->first = list->last = node;
    } else {
        list->last->next = node;
        list->last = node;
    }
    pthread_mutex_unlock(&list->mutex);

    return node;
}

void list_destroy(List* list) {
    Node* node = list->first;
    while (node) {
        Node* next = node->next;
        pthread_rwlock_destroy(&node->sync);
        pthread_cond_destroy(&node->cond);
        free(node->value);
        free(node);
        node = next;
    }
    pthread_mutex_destroy(&list->mutex);
    free(list);
}
