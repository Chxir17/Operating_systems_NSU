#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ucontext.h>

#include "u_thread.h"
#include <sys/time.h>

long long now_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + (long long)tv.tv_usec;
}

int u_thread_startup(void *arg, void *u_thread_manager) {
    u_thread_struct_t *u_thread = arg;
    u_thread_manager_t *manager = u_thread_manager;
    void *retval = NULL;

    printf("uthread_startup: starting a thread func for the thread %d\n", u_thread->u_thread_id);
    u_thread->thread_func(u_thread->arg, manager);

    u_thread->retval = retval;
    u_thread->finished = 1;

    return 0;
}

void u_thread_usleep(u_thread_manager_t *mgr, long long usec) {
    u_thread_struct_t *cur = mgr->uthreads[mgr->u_thread_cur];
    cur->sleep_until_us = now_us() + usec;
    u_thread_scheduler(mgr);
}

void *create_stack(off_t size) {
    void *stack = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    return stack;
}

void u_thread_scheduler(u_thread_manager_t *mgr) {
    ucontext_t *cur_ctx = &mgr->uthreads[mgr->u_thread_cur]->u_ctx;
    int next = mgr->u_thread_cur;

    long long t = now_us();

    for (;;) {
        next = (next + 1) % mgr->u_thread_count;

        u_thread_struct_t *thr = mgr->uthreads[next];

        if (thr->finished)
            continue;
        if (thr->sleep_until_us > t)
            continue;

        break;
    }

    mgr->u_thread_cur = next;

    ucontext_t *next_ctx = &mgr->uthreads[next]->u_ctx;

    if (swapcontext(cur_ctx, next_ctx) == -1) {
        perror("sheduler swapcontext");
        exit(1);
    }
}

int u_thread_create(u_thread_t *u_thread, u_thread_manager_t *u_thread_manager, void (*thread_func), void *arg) {
    static int u_thread_id = 0;

    u_thread_id++;

    printf("u_thread_create: creating thread %d\n", u_thread_id);

    void *stack = create_stack(STACK_SIZE);
    if (stack == NULL) {
        fprintf(stderr, "create_stack() failed\n");
        return -1;
    }
    u_thread_t new_u_thread = (u_thread_struct_t *) (stack + STACK_SIZE - sizeof(u_thread_struct_t));

    int err = getcontext(&new_u_thread->u_ctx);
    if (err == -1) {
        perror("u_thread_create: getcontext() failed:");
        return -1;
    }

    new_u_thread->u_ctx.uc_stack.ss_sp = stack;
    new_u_thread->u_ctx.uc_stack.ss_size = STACK_SIZE - sizeof(u_thread_t);
    new_u_thread->u_ctx.uc_link = &u_thread_manager->uthreads[0]->u_ctx;

    makecontext(&new_u_thread->u_ctx, (void (*)(void)) u_thread_startup, 2, new_u_thread, u_thread_manager);

    new_u_thread->u_thread_id = u_thread_id;
    new_u_thread->thread_func = thread_func;
    new_u_thread->arg = arg;
    new_u_thread->finished = 0;

    u_thread_manager->uthreads[u_thread_manager->u_thread_count] = new_u_thread;
    u_thread_manager->u_thread_count++;

    *u_thread = new_u_thread;

    return 0;
}

int thread_is_finished(u_thread_t u_tid) {
    if (u_tid->finished) {
        return 1;
    }
    return 0;
}

void init_thread(u_thread_t *main_thread, u_thread_manager_t *mgr) {
    getcontext(&(*main_thread)->u_ctx);

    (*main_thread)->u_thread_id = 0;
    (*main_thread)->thread_func = NULL;
    (*main_thread)->arg = NULL;
    (*main_thread)->finished = 0;
    (*main_thread)->sleep_until_us = 0;

    mgr->uthreads[0] = *main_thread;
    mgr->u_thread_count = 1;
    mgr->u_thread_cur = 0;
}