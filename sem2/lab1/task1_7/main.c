//Контекст — это сохранённое состояние выполнения программы, которое позволяет приостановить выполнение
//и потом продолжить выполнение с того же места

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "u_thread.h"

void thread1(void *arg, u_thread_manager_t *u_thread_manager) {
    char *str = arg;
    for (int i = 0; i < 5; i++) {
        printf("thread1[%d]: %s\n", i, str);
        u_thread_usleep(u_thread_manager, 1000000);
    }
}

void thread2(void *arg, u_thread_manager_t *u_thread_manager) {
    char *str = arg;
    for (int i = 0; i < 5; i++) {
        printf("thread2[%d]: %s\n", i, str);
        u_thread_usleep(u_thread_manager, 2000000);
    }
}

void thread3(void *arg, u_thread_manager_t *u_thread_manager) {
    char *str = arg;
    for (int i = 0; i < 5; i++) {
        printf("thread3[%d]: %s\n", i, str);
        u_thread_usleep(u_thread_manager, 500000);
    }
}

int main() {
    u_thread_t my_threads[4];
    u_thread_t main_thread;
    u_thread_manager_t *u_thread_manager = malloc(sizeof(u_thread_manager_t));

    init_thread(&main_thread, u_thread_manager);
    my_threads[0] = main_thread;
    printf("main [%d]\n", getpid());

    int err = u_thread_create(&my_threads[1], u_thread_manager, thread1, "hello from main1");
    if (err == -1) {
        fprintf(stderr, "u_thread_create() failed\n");
    }
    err = u_thread_create(&my_threads[2], u_thread_manager, thread2, "hello from main2");
    if (err == -1) {
        fprintf(stderr, "u_thread_create() failed\n");
    }
    err = u_thread_create(&my_threads[3], u_thread_manager, thread3, "hello from main3");
    if (err == -1) {
        fprintf(stderr, "u_thread_create() failed\n");
    }

    while (1) {
        int count = 0;
        for (int i = 1; i < 4; i++) {
            if (thread_is_finished(my_threads[i])) {
                count++;
            }
        }
        if (count == 3) {
            break;
        }
        u_thread_scheduler(u_thread_manager);
    }
    free(u_thread_manager);
    return 0;
}