#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

void *mythread(void *arg) {
    // pthread_detach(pthread_self());
    printf("mythread [%d %d %d]: Hello from mythread!\n", getpid(), getppid(), gettid());
    printf("%lu\n", pthread_self());
    return NULL;
}


int main() {
    pthread_t tid;
    int err;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());
    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (err) {
        printf("main: pthread_attr_setdetachstate() failed: %s\n", strerror(err));
        return -1;
    }



    while (1) {
        //err = pthread_create(&tid, &attr, mythread, NULL);
        err = pthread_create(&tid, NULL, mythread, NULL);
        if (err) {
            printf("main: pthread_create() failed: %s\n", strerror(err));
            return -1;
        }
    }
    err = pthread_attr_destroy(&attr);
    if (err) {
        printf("main: pthread_attr_destroy() failed: %s\n", strerror(err));
    }
    return 0;
}
