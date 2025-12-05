#ifndef U_THREAD_H
#define U_THREAD_H

#include <ucontext.h>

#define PAGE 4096
#define STACK_SIZE (PAGE * 5)
#define MAX_THREADS 10

typedef struct _u_thread_struct_t {
    int              u_thread_id;
    void             (*thread_func)(void*, void*);
    void             *arg;
    void             *retval;

    volatile int     finished;
    long long        sleep_until_us;
    ucontext_t       u_ctx;
} u_thread_struct_t;

typedef u_thread_struct_t *u_thread_t;

typedef struct _u_thread_manager_t {
    u_thread_struct_t *uthreads[MAX_THREADS];
    int u_thread_count;
    int u_thread_cur;
} u_thread_manager_t;

void u_thread_usleep(u_thread_manager_t *mgr, long long usec);
int u_thread_create(u_thread_t *u_thread, u_thread_manager_t *u_thread_manager, void (*thread_func), void *arg);
void u_thread_scheduler(u_thread_manager_t *u_thread_manager);
int thread_is_finished(u_thread_t u_tid);
void init_thread(u_thread_t *main_thread, u_thread_manager_t *u_thread_manager);

#endif