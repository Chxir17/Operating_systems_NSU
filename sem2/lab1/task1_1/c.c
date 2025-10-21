#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

#define NUM_THREADS 5

int gvar = 10;

void *mythread(void *arg) {
    static int svar = 20;
    const int cvar = 30;
    int lvar = 40;

    pthread_t self_id = pthread_self();

    printf("Thread [PID=%d, PPID=%d, TID=%d, pthread_self=%ld]\n"
           "Addresses: lvar=%p, svar=%p, cvar=%p, gvar=%p\n", getpid(), getppid(), gettid(), self_id, &lvar, &svar, &cvar, &gvar);
    pthread_t *passed_id = (pthread_t *)arg;
    sleep(6);
    if (passed_id != NULL) {
        if (pthread_equal(*passed_id, self_id)) {
            printf("pthread_equal: YES, pthread_self() matches passed ID\n");
        } else {
            printf("pthread_equal: NO, pthread_self() differs from passed ID\n");
        }
    }

    return NULL;
}

int main() {
    pthread_t tids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        int err = pthread_create(&tids[i], NULL, mythread, &tids[i]);
        if (err) {
            for (int j = 0; j < i; j++) {
                pthread_join(tids[j], NULL);
            }
            printf("pthread_create %d failed with error %d\n", i, err);
            return -1;
        }
        printf("%d - tid (from main) = %ld\n", i, tids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(tids[i], NULL);
    }

    printf("main [%d %d %ld]: Hello from main!\n", getpid(), getppid(), syscall(SYS_gettid));
    return 0;
}
