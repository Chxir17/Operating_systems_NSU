#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void *mythread(void *args){
    while (1){
        printf("thread id: %p\n", pthread_self());
        sleep(1);
    }
}

int main(){
    pthread_t tid;
    int err;

    err = pthread_create(&tid, NULL, mythread, NULL);
    if (err){
        printf("main: pthread_create() failed: %s\n", strerror(err));
        return -1;
    }

    printf("main: thread created\n");
    sleep(5);

    err = pthread_cancel(tid);
    if (err) {
        perror("main: error canceling thread");
        return -1;
    }

    printf("main: thread cancel signal sent\n");

    int result;
    int *resultPtr = &result;

    err = pthread_join(tid, (void **)&resultPtr);
    if (err){
        perror("main: error joining thread");
        return -1;
    }

    return 0;
}