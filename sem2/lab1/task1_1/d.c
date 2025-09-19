#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#define NUM_THREADS 5
#include <sys/syscall.h>

int gvar = 10;

void *mythread(void *arg) {
    static int svar = 20;
    const int cvar = 30;
    int lvar = 40;
    printf("Thread [PID=%d, PPID=%d, TID=%d, pthread_self=%ld]\nAddresses: lvar=%p, svar=%p, cvar=%p, gvar=%p\nVaribles: lvar=%d, gvar=%d\n", getpid(), getppid(), gettid(), pthread_self(), &lvar, &svar, &cvar, &gvar, lvar, gvar);
    sleep(90);
    lvar++;
    gvar++;
    return NULL;
}

int main() {

    pthread_t tids[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        int err = pthread_create(&tids[i], NULL, mythread, NULL);
        if (err) {
            for (int j = 0; j < i; j++) {
                pthread_join(tids[j], NULL);
            }
            printf("pthread_create %d failed with error %d\n",i , err);
            return -1;
        }
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(tids[i], NULL);
    }
    printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());
    return 0;
}
