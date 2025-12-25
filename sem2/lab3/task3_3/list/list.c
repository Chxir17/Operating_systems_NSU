#include <string.h>
#include "list.h"

Map* map_init() {
    Map* map = malloc(sizeof(Map));
    if (!map) {
        perror("Can't allocate memory for map");
        abort();
    }
    map->first = NULL;
    if (pthread_mutex_init(&map->mutex, NULL) != 0) {
        perror("Can't initialize mutex");
        abort();
    }
    return map;
}

List* list_init() {
    List* list = malloc(sizeof(List));
    if (!list) {
        perror("Can't allocate memory for list");
        abort();
    }
    list->first = NULL;
    return list;
}

Cache* map_add(Map* map, const char* url) {
    Cache* newCache = malloc(sizeof(Cache));
    if (!newCache) {
        perror("Can't allocate memory for cache node");
        abort();
    }

    long length = strlen(url) + 1;
    newCache->url = (char*)malloc(length + 1);
    if (!newCache->url) {
        perror("Can't allocate memory for cache");
        abort();
    }
    memcpy(newCache->url, url, length);

    newCache->url[length] = '\0';
    newCache->response = list_init();
    newCache->next = map->first;
    map->first = newCache;
    return map->first;
}

Cache* map_find_by_url(const Map* map, const char* url) {
    Cache* current = map->first;
    while (current != NULL) {
        if (strcmp(current->url, url) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

Node* list_add(List* list, const char* value, long length) {
    Node* newNode = malloc(sizeof(Node));
    if (!newNode) {
        perror("Can't allocate memory for list node");
        abort();
    }

    newNode->size = length;
    newNode->value = (char*)malloc(length + 1);
    if (!newNode->value) {
        perror("Can't allocate memory for list node");
        free(newNode);
        abort();
    }

    memcpy(newNode->value, value, length);
    newNode->value[length] = '\0';
    if (pthread_rwlock_init(&newNode->sync, NULL) != 0) {
        perror("Can't initialize rwlock");
        abort();
    }
    newNode->next = NULL;
    if (list->first == NULL) {
        list->first = newNode;
        list->last = newNode;
    } else {
        list->last->next = newNode;
        list->last = newNode;
    }
    return newNode;
}

void list_print(const List* list) {
    const Node* current = list->first;
    while (current != NULL) {
        printf("%s\n", current->value);
        current = current->next;
    }
}

void list_destroy(List* list) {
    Node* current = list->first;
    while (current != NULL) {
        Node* next = current->next;
        pthread_rwlock_destroy(&current->sync);
        free(current);
        current = next;
    }
    free(list);
}