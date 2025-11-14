#define _GNU_SOURCE
#include "my_thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *my_thread_func(void *arg) {
    printf("mythread [%d %d %d]\n", getpid(), getppid(), gettid());
    printf("Thread starts with args: %s\n", (char *)arg);
    int *res = malloc(sizeof(int));
    *res = 10;
    return res;
}

int main()
{
    mythread_t thread;
    char *arg = "Hi!";
    printf("         [PID  PPID  TID]\n");

    printf("main     [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());

    if (mythread_create(&thread, my_thread_func, arg) == -1)
    {
        printf("Cannot create thread");
        return 1;
    }
    void *retval;
    mythread_join(&thread, &retval);
    int result = *(int *)retval;
    free(retval);
    sleep(5);
    printf("Thread finished with arg %d\n", result);

    return 0;
}