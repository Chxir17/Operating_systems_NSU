#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <linux/futex.h>
#include <sys/syscall.h>

#include "my_spinlock.h"

struct my_spinlock {
    int lock;
};

void my_spinlock_init(my_spinlock_t *lock) {
    *lock = 1;
}

void my_spinlock_lock(my_spinlock_t *lock) {
    while (1) {
        int expected = 1;
        if (atomic_compare_exchange(lock, &expected, 0)) {
            break;
        }
    }
}

void my_spinlock_unlock(my_spinlock_t *lock) {
    const int expected = 0;
    atomic_compare_exchange(lock, &expected, 1);
}