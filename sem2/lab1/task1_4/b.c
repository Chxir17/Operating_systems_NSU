
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

void *mythread(void *args){
    long long counter = 0;
//    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
//    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    while (1){
        counter++;
//        pthread_testcancel();
    }
    return NULL;
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
    if (err){
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
