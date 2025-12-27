#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>

#include "request.h"

const char *list_get_key(struct METADATA_HEAD *list, const char *key) {
    Metadata_item *item;
    TAILQ_FOREACH(item, list, entries) {
        if (strcmp(item->key, key) == 0) {
            return item->value;
        }
    }
    return NULL;
}

void list_add_key(struct METADATA_HEAD *list, const char *key, const char *value) {
    Metadata_item *item = malloc(sizeof(Metadata_item));
    item->key = key;
    item->value = value;
    TAILQ_INSERT_TAIL(list, item, entries);
}