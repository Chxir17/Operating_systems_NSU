#define _GNU_SOURCE
#include "my_thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *my_thread_func(void *arg) {
    printf("mythread: PID:%d PPID:%d TID:%d]\n", getpid(), getppid(), gettid());
    printf("Thread starts with args: %s\n", (char *)arg);
    int *res = malloc(sizeof(int));
    *res = 0;
    return res;
}

int main() {
    my_thread_t thread;
    char *arg = "Hello world!";
    printf("main: PID:%d PPID:%d TID:%d]\n", getpid(), getppid(), gettid());
    int err = my_thread_create(&thread, my_thread_func, arg);
    if (err != 0) {
        printf("Cannot create thread");
        return 1;
    }
    void *retval;
    my_thread_join(&thread, &retval);
    int result = *(int *)retval;
    free(retval);
    printf("Thread finished with result %d\n", result);
    return 0;
}