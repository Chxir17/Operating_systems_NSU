#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    int a;
    char *b;
} myStruct;

void *mythread(void *args) {
    myStruct *structure = args;
    printf("mythread [%d %d %d]: Hello from mythread!\nstruct vals: %d; %s\n", getpid(), getppid(), gettid(), structure->a, structure->b);
    return NULL;
}

int main() {
    myStruct structure = {42, "Hello, world!"};
    printf("main: struct initialized with %d, %s\n", structure.a, structure.b);
    pthread_t tid;
    int err;
    err = pthread_create(&tid, NULL, mythread, &structure);
    if (err) {
        printf("main: pthread_create() failed: %s\n", strerror(err));
        return -1;
    }
    err = pthread_join(tid, NULL);
    if (err) {
        printf("main: pthread_join() failed: %s\n", strerror(err));
        return -1;
    }
    return 0;
}