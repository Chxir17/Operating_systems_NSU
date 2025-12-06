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

long long now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
}

void u_thread_usleep(u_thread_manager_t *mgr, long long usec) {
    if (mgr->u_thread_cur == 0) {
        u_thread_scheduler(mgr);
        return;
    }
    u_thread_struct_t *cur = mgr->uthreads[mgr->u_thread_cur];
    cur->sleep_until_us = now_us() + usec;
    u_thread_scheduler(mgr);
}

void u_thread_scheduler(u_thread_manager_t *mgr)
{
    u_thread_t cur = mgr->cur;
    long long t = now_us();

    do {
        cur = cur->next;
        if (cur->finished && cur != mgr->head) {
            u_thread_t to_delete = cur;
            cur = cur->next;
            to_delete->prev->next = to_delete->next;
            to_delete->next->prev = to_delete->prev;

            if (mgr->head == to_delete)
                mgr->head = cur;

            munmap(to_delete->stack_base, STACK_SIZE);

            mgr->count--;
            if (mgr->count == 0) {
                fprintf(stderr, "NO THREADS LEFT!\n");
                exit(1);
            }

            continue;
        }

        if (!cur->finished && cur->sleep_until_us <= t)
            break;

    } while (cur != mgr->cur);

    if (cur != mgr->cur) {
        u_thread_t old = mgr->cur;
        mgr->cur = cur;
        swapcontext(&old->u_ctx, &cur->u_ctx);
    }
}


int thread_is_finished(u_thread_t u_tid) {
    if (u_tid->finished) {
        return 1;
    }
    return 0;
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

void *create_stack(off_t size) {
    void *stack = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    return stack;
}

int u_thread_create(u_thread_t *out, u_thread_manager_t *mgr, void (*func)(void*, void*), void *arg){
    static int u_thread_id = 0;
    u_thread_id++;
    printf("u_thread_create: creating thread %d\n", u_thread_id);
    void *stack = create_stack(STACK_SIZE);
    if (stack == NULL) {
        fprintf(stderr, "create_stack() failed\n");
        return -1;
    }

    u_thread_t thr = (u_thread_t)((char*)stack + STACK_SIZE - sizeof(*thr));
    int err = getcontext(&thr->u_ctx);
    if (err == -1) {
        perror("u_thread_create: getcontext() failed:");
        return -1;
    }

    thr->u_ctx.uc_stack.ss_sp = stack;
    thr->u_ctx.uc_stack.ss_size = STACK_SIZE - sizeof(*thr);
    thr->u_ctx.uc_link = &mgr->head->u_ctx;
    makecontext(&thr->u_ctx, (void(*)(void))u_thread_startup, 2, thr, mgr);

    thr->u_thread_id = u_thread_id++;
    thr->thread_func = func;
    thr->arg = arg;
    thr->finished = 0;
    thr->sleep_until_us = 0;
    thr->stack_base = stack;
    u_thread_t after = mgr->cur->next;
    thr->next = after;
    thr->prev = mgr->cur;
    mgr->cur->next = thr;
    after->prev = thr;
    mgr->count++;
    *out = thr;
    return 0;
}

void init_thread(u_thread_t *main_thread, u_thread_manager_t *mgr) {
    u_thread_struct_t *mt = malloc(sizeof(u_thread_struct_t));
    if (!mt) {
        perror("malloc main thread");
        exit(1);
    }

    if (getcontext(&mt->u_ctx) == -1) {
        perror("getcontext for main thread");
        exit(1);
    }

    mt->u_thread_id = 0;
    mt->finished = 0;
    mt->stack_base = NULL;

    mt->next = mt;
    mt->prev = mt;

    mgr->head = mt;
    mgr->cur  = mt;
    mgr->count = 1;

    *main_thread = mt;
}