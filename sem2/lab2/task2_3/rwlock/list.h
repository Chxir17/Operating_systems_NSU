//
// Created by Ilya Zyryanov on 18.12.2025.
//

#ifndef OS_LIST_H
#define OS_LIST_H

#define _GNU_SOURCE
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>

#define MAX_STR 100

typedef struct Node {
    char value[MAX_STR];
    struct Node *next;
    pthread_rwlock_t sync;
} Node;

typedef struct List {
    Node *start;
    long long length;
} List;

struct ThreadArg {
    List *l;
    int thread_id;
};

List* list_init(long long n);
void list_destroy(List* l);

void* increasing_thread(void* arg);
void* decreasing_thread(void* arg);
void* equal_thread(void* arg);
void* swap_thread(void* arg);
void* monitor_thread(void* arg);

extern long long increasing_iterations;
extern long long decreasing_iterations;
extern long long equals_iterations;
extern long long swap_success[3];
extern atomic_int stop_flag;

#endif // OS_LIST_H
