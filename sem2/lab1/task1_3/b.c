#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

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
    myStruct *structure = malloc(sizeof(myStruct));
    if (!structure) {
        perror("malloc");
        return -1;
    }
    structure->a = 42;
    structure->b = "Hello, world!";
    printf("main: struct initialized with %d, %s\n", structure->a, structure->b);
    pthread_t tid;
    pthread_attr_t attr;
    int err;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    err = pthread_create(&tid, &attr, mythread, structure);
    if (err){
        printf("main: pthread_create() failed: %s\n", strerror(err));
        free(structure);
        return -1;
    }
    err = pthread_attr_destroy(&attr);
    if (err) {
        printf("main: pthread_aatr_destroy failed: %s\n", strerror(err));
    }
    free(structure);
    return 0;
}