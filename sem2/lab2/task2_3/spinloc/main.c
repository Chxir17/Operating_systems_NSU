#include "list.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

int main() {
    long n = 100;
    List *l = list_init(n);
    struct ThreadArg targ = {l, 0};
    pthread_t t_increasing, t_decreasing, t_equal;
    int err1 = pthread_create(&t_increasing, NULL, increasing_thread, &targ);
    int err2 = pthread_create(&t_decreasing, NULL, decreasing_thread, &targ);
    int err3 = pthread_create(&t_equal, NULL, equal_thread, &targ);
    if (err1 || err2 || err3) {
        perror("Error creating thread");
        return -1;
    }
    pthread_t t_swap1, t_swap2, t_swap3;
    struct ThreadArg targ1 = {l, 0};
    struct ThreadArg targ2 = {l, 1};
    struct ThreadArg targ3 = {l, 2};
    err1 = pthread_create(&t_swap1, NULL, swap_thread, &targ1);
    err2 = pthread_create(&t_swap2, NULL, swap_thread, &targ2);
    err3 = pthread_create(&t_swap3, NULL, swap_thread, &targ3);
    if (err1 || err2 || err3) {
        perror("Error creating thread");
        return -1;
    }
    pthread_t monitor;
    err1 = pthread_create(&monitor, NULL, monitor_thread, &targ);
    if (err1) {
        perror("Error creating thread");
        return -1;
    }
    sleep(15);


    atomic_store(&stop_flag, 1);
    err1 = pthread_join(t_increasing, NULL);
    err2 = pthread_join(t_decreasing, NULL);
    err3 = pthread_join(t_equal, NULL);
    if (err1 || err2 || err3) {
        perror("Error joining thread");
    }
    err1 = pthread_join(t_swap1, NULL);
    err2 = pthread_join(t_swap2, NULL);
    err3 = pthread_join(t_swap3, NULL);
    if (err1 || err2 || err3) {
        perror("Error joining thread");
    }
    err1 = pthread_join(monitor, NULL);
    if (err1) {
        perror("Error joining thread");
    }

    printf("increasing iterations: %lld\n", increasing_iterations);
    printf("decreasing iterations: %lld\n", decreasing_iterations);
    printf("equals iterations: %lld\n", equals_iterations);
    printf("swap successes: %lld %lld %lld\n", swap_success[0], swap_success[1], swap_success[2]);

    list_destroy(l);
    return 0;
}