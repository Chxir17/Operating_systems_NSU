#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sched.h>
#include <string.h>
#include <pthread.h>
#include <linux/futex.h>
#include <syscall.h>
#include "my_thread.h"
#include <stdatomic.h>

typedef struct cleanup_node {
    my_thread_t thread;
    struct cleanup_node *next;
} cleanup_node_t;

static cleanup_node_t *cleanup_list = NULL;
static pthread_mutex_t cleanup_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cleanup_cond = PTHREAD_COND_INITIALIZER;
static _Atomic int cleanup_thread_started = 0;


void *cleanup_thread_func(void *arg) {
    while (1) {
        pthread_mutex_lock(&cleanup_mutex);
        while (cleanup_list == NULL) {
            pthread_cond_wait(&cleanup_cond, &cleanup_mutex);
        }

        cleanup_node_t *node = cleanup_list;
        cleanup_list = cleanup_list->next;
        pthread_mutex_unlock(&cleanup_mutex);

        if (node) {
            my_thread_t thread = node->thread;
            while (!thread->is_finished) {
                sleep(10);
            }
            munmap(thread->stack, getpagesize());
            free(node);
        }
    }
}

typedef void *(*start_routine_t)(void *);

static _Atomic(int) ids = 0;

void thread_func(void *arg) {
    my_thread_t my_thread = arg;
    my_thread->retval = my_thread->start_routine(my_thread->arg);
    my_thread->is_finished = 1;
    syscall(SYS_futex, &my_thread->futex, FUTEX_WAKE, 1, NULL, NULL, 0);
    syscall(SYS_exit, 0);
}

int my_thread_create(my_thread_t *thread, start_routine_t routine, void *arg) {

    const int STACK_SIZE = getpagesize();
    ids++;
    //адрес, размер, права, привязка к файлу, дескриптор файла, смешение в файле
    void *region = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap failed");
        ids--;
        return -1;
    }

    my_thread_t t = (my_thread_t)((char *)region + STACK_SIZE - sizeof(thread_struct));
    t->id = ids;
    t->stack = region;
    t->stack_size = STACK_SIZE;
    t->start_routine = routine;
    t->arg = arg;
    t->retval = NULL;
    t->is_finished = 0;
    t->futex = 0;
    t->joinable = 1;


    //VM - общая память с родителем FS - общие системные атрибуты FILES - общие дескрипторы // SIGHAND - общие обработчики сигналов //Thread - поток тогоже процесса
    int pid = clone(thread_func, (void *)(char *)t, CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD, t);
    if (pid == -1) {
        perror("clone failed");
        munmap(region, STACK_SIZE);
        return -1;
    }
    t->tid = pid;
    *thread = t;
    return 0;
}

void my_thread_join(my_thread_t *thread, void **retval){
    if (!(*thread)->joinable) {
        perror("Error: Cannot join a detached thread\n");
        return;
    }
    while (!(*thread)->is_finished) {
        syscall(SYS_futex, &(*thread)->futex, FUTEX_WAIT, 1, NULL, NULL, 0);
    }
    if (retval != NULL) {
        *retval = (*thread)->retval;
    }
    (*thread)->futex = 0;
    munmap((*thread)->stack, getpagesize());
    printf("thread joined\n");
}

int my_thread_detach(my_thread_t *thread) {
    if (*thread == NULL) {
        return -1;
    }
    if (!(*thread)->joinable) {
        perror("Error: Thread already detached\n");
        return -1;
    }

    (*thread)->joinable = 0;
    printf("thread detached\n");
    if (atomic_exchange(&cleanup_thread_started, 1) == 0) {
        my_thread_t cleaner_thread;
        if (my_thread_create(&cleaner_thread, cleanup_thread_func, NULL) != 0) {
            perror("Error: failed to start cleanup thread\n");
            return -1;
        }
    }

    cleanup_node_t *node = malloc(sizeof(cleanup_node_t));
    node->thread = *thread;
    pthread_mutex_lock(&cleanup_mutex);
    node->next = cleanup_list;
    cleanup_list = node;
    pthread_mutex_unlock(&cleanup_mutex);
    pthread_cond_signal(&cleanup_cond);
    return 0;
}