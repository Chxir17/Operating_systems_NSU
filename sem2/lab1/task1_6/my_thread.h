#pragma once
#include <stdatomic.h>

typedef void *(*start_routine_t)(void *);

typedef struct thread_struct {
    int id;
    int tid;
    void *stack;
    size_t stack_size;

    void *(*start_routine)(void *);
    void *arg;
    void *retval;

    _Atomic int is_finished;
    _Atomic int joinable;
    _Atomic int futex;
} thread_struct;


typedef thread_struct* mythread_t;
int mythread_create(mythread_t *thread, start_routine_t routine, void *arg);
void mythread_join(mythread_t *thread, void** retval);
int mythread_detach(mythread_t *thread);