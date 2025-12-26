#include <string.h>
#include "list.h"

int list_wait_for_data(List* list, Node** current) {
    pthread_mutex_lock(&list->mutex);
    while (*current == list->last && !list->complete) {
        // Ждём нового чанка или завершения
        if (pthread_cond_wait(&list->cond, &list->mutex) != 0) {
            pthread_mutex_unlock(&list->mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&list->mutex);
    return 1;
}

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
    list->last = NULL;
    list->complete = 0;
    if (pthread_mutex_init(&list->mutex, NULL) != 0 ||
        pthread_cond_init(&list->cond, NULL) != 0) {
        perror("Can't initialize list sync");
        abort();
        }
    return list;
}

Cache* map_add(Map* map, const char* url) {
    Cache* newCache = malloc(sizeof(Cache));
    if (!newCache) abort();

    newCache->url = strdup(url);
    newCache->response = list_init();
    newCache->next = map->first;
    map->first = newCache;
    return newCache;
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
        perror("Can't allocate memory for list node value");
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
    pthread_mutex_lock(&list->mutex);
    if (list->first == NULL) {
        list->first = list->last = newNode;
    } else {
        list->last->next = newNode;
        list->last = newNode;
    }
    pthread_cond_broadcast(&list->cond);
    pthread_mutex_unlock(&list->mutex);

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