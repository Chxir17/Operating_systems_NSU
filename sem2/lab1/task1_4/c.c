#define _GNU_SOURCE
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void free_mem(void *args){
    printf("cleaning memory...\n");
    free(args);
}

void *mythread(void *args){
    char *hello_str = malloc(sizeof(char) * 14);
    strcpy(hello_str, "Hello, World!");
    pthread_cleanup_push(free_mem, hello_str);//2
    pthread_cleanup_push(free_mem, hello_str);//1
    while (1){
        printf("thread: %s\n", hello_str);
        sleep(1);
    }
    pthread_cleanup_pop(1);//1
    pthread_cleanup_pop(1);//2
}

int main(){
    pthread_t tid;

    int err = pthread_create(&tid, NULL, mythread, NULL);
    if (err){
        printf("main: pthread_create() failed: %s\n", strerror(err));
        return -1;
    }

    printf("main: thread created\n");

    sleep(3);

    err = pthread_cancel(tid);
    if (err){
        perror("main: error canceling thread");
        return -1;
    }

    printf("main: sent thread cancel request\n");

    int *result;
    err = pthread_join(tid, (void **)&result);
    if (err){
        perror("main: error joining thread");
        return -1;
    }

    printf("main: pthread canceled: %d\n", result == PTHREAD_CANCELED);
    return 0;
}