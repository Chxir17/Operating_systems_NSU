#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>


void sigint_handler(int arg) {
    printf("handler: %p\n", pthread_self());
    printf("SIGINT HANDLER: got sigint (%d),  %d\n", SIGINT, arg);
}

void *my_thread_sigint(void *args) {
    printf("thread SIGINT: %p\n", pthread_self());
    signal(SIGINT, sigint_handler);
    if (errno) {
        perror("thread SIGINT: error setting sigint handler");
        return NULL;
    }
    printf("thread SIGINT: waiting for SIGINT\n");
    while (1) {
        sleep(1);
    }
}

void *my_thread_sigquit(void *args) {
    printf("thread SIGQUIT: %p\n", pthread_self());
    sigset_t set;
    int signal_code;
    int err = sigemptyset(&set);
    if (err) {
        perror("thread SIGQUIT: error setting empty set");
        return NULL;
    }
    err = sigaddset(&set, SIGQUIT);
    if (err) {
        perror("thread SIGQUIT: error adding SIGQUIT to set");
        return NULL;
    }
    if (sigprocmask(SIG_SETMASK, &set, NULL) < 0) {
        perror("thread SIGQUIT: error sigprocmask");
        return NULL;
    }
    printf("thread SIGQUIT: waiting for SIGQUIT\n");
    while (1) {
        err = sigwait(&set, &signal_code);
        if (err) {
            perror("thread SIGQUIT: error during sigwait");
            return NULL;
        }

        if (signal_code == SIGQUIT) {
            printf("thread SIGQUIT: got SIGQUIT\n");
        }
    }
}

void *thread_foo_immortal(void *args) {
    printf("thread 3: %p\n", pthread_self());
    sigset_t set;
    int err = sigfillset(&set);
    if (err) {
        perror("main: error filing sigset for thread 1");
        return NULL;
    }
    if (sigprocmask(SIG_SETMASK, &set, NULL) < 0) {
        perror("thread SIGQUIT: error sigprocmask");
        return NULL;
    }
    sleep(20);
    printf("thread 3: I AM ALIVE\n");
    return NULL;
}


int main() {

    // sigset_t set;
    // sigemptyset(&set);
    // sigaddset(&set, SIGQUIT);
    // sigprocmask(SIG_BLOCK, &set, NULL);

    printf("thread main: %p\n", pthread_self());
    pthread_t tid1, tid2, tid3;

    int err = pthread_create(&tid1, NULL, my_thread_sigint, NULL);
    if (err) {
        perror("main: error creating thread 1");
        return -1;
    }
    printf("main: thread SIGINT started\n");

    err = pthread_create(&tid2, NULL, my_thread_sigquit, NULL);
    if (err) {
        perror("main: error creating thread 2");
        return -1;
    }
    printf("main: thread SIGQUIT started\n");

    err = pthread_create(&tid3, NULL, thread_foo_immortal, NULL);
    if (err) {
        perror("main: error creating thread 2");
        return -1;
    }
    printf("main: thread immortal started\n");

    sleep(10);

    err = pthread_kill(tid1, SIGINT);
    if (err) {
        perror("main: error killing thread");
        return -1;
    }
    printf("main: SIGINT sent\n");

    err = pthread_kill(tid2, SIGQUIT);
    if (err) {
        perror("main: error killing thread");
        return -1;
    }
    printf("main: SIGQUIT sent\n");

    err = pthread_kill(tid3, SIGSEGV);
    if (err) {
        perror("main: error killing thread");
        return -1;
    }
    printf("main: SIGSEGV sent\n");

    sleep(30);

    err = pthread_kill(tid1, SIGKILL);
    if (err) {
        perror("main: error killing thread");
        return -1;
    }

    err = pthread_kill(tid2, SIGKILL);
    if (err) {
        perror("main: error killing thread");
        return -1;
    }

    err = pthread_kill(tid3, SIGKILL);
    if (err) {
        perror("main: error killing thread");
        return -1;
    }

    err = pthread_join(tid1, NULL);
    if (err) {
        perror("main: error joining thread 1");
        return -1;
    }

    err = pthread_join(tid2, NULL);
    if (err) {
        perror("main: error joining thread 2");
        return -1;
    }
    err = pthread_join(tid3, NULL);
    if (err) {
        perror("main: error joining thread 3");
        return -1;
    }
    pthread_exit(0);
}