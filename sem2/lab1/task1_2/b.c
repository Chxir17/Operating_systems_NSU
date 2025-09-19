#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

void *mythread(void *arg) {
    *((int *)arg) = 42;
    printf("mythread [%d %d %d]: Hello from mythread!\n", getpid(), getppid(), gettid());
    return arg;
}

int main() {
    pthread_t tid;
    int err;
    int value;
    int *valueP = &value;
    printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());
    err = pthread_create(&tid, NULL, mythread, &value);
    if (err) {
        printf("main: pthread_create() failed: %s\n", strerror(err));
        return -1;
    }
    err = pthread_join(tid, (void **)&valueP);
    if (err) {
        printf("main: pthread_join() failed: %s\n", strerror(err));
        return -1;
    }
    printf("42 = %d\n", value);
    return 0;
}
