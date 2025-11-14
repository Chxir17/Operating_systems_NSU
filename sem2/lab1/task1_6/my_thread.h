#pragma once

typedef void *(*start_routine_t)(void *);


typedef struct thread_struct {
    int id;
    int tid;
    void *stack;
    long long stack_size;
    void *(*start_routine)(void *);
    void *arg;
    void *retval;
    _Atomic int is_finished;
    _Atomic int joinable;
    _Atomic int futex;
} thread_struct;


typedef thread_struct* my_thread_t;
int my_thread_create(my_thread_t *thread, start_routine_t routine, void *arg);
void my_thread_join(my_thread_t *thread, void** retval);
int my_thread_detach(my_thread_t *thread);