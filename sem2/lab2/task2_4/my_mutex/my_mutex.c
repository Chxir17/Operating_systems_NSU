#define _GNU_SOURCE

#include "my_mutex.h"

void my_mutex_init(my_mutex_t* m, my_mutex_type_t type) {
    atomic_store(&m->lock, 1);
    m->owner = 0;
    m->recursion = 0;
    m->type = type;
}

int futex(int* uaddr, int futex_op, int val, const struct timespec* timeout, int* uaddr2, int val3) {
    return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

void my_mutex_lock(my_mutex_t* m) {
    pid_t tid = get_tid();
    if (atomic_load(&m->lock) == 0 && m->owner == tid) {
        switch (m->type) {
            case MY_MUTEX_RECURSIVE:
                m->recursion++;
                return;
            case MY_MUTEX_ERRORCHECK:
                errno = EDEADLK;
                perror("my_mutex_lock (ERRORCHECK)");
                return;
            case MY_MUTEX_NORMAL:
                fprintf(stderr, "my_mutex_lock: deadlock detected (NORMAL mutex)\n");
                abort();
        }
    }

    while (1) {
        int expected = 1;
        if (atomic_compare_exchange_strong(&m->lock, &expected, 0)) {
            m->owner = tid;
            m->recursion = 1;
            return;
        }

        int err = futex(&m->lock, FUTEX_WAIT, 0, NULL, NULL, 0);
        if (err == -1 && errno != EAGAIN) {
            perror("futex WAIT");
            abort();
        }
    }
}


void my_mutex_unlock(my_mutex_t* m) {
    pid_t tid = get_tid();
    if (m->owner != tid) {
        if (m->type == MY_MUTEX_ERRORCHECK) {
            errno = EPERM;
            perror("my_mutex_unlock");
            abort();
        }
        return;
    }
    if (m->type == MY_MUTEX_RECURSIVE && --m->recursion > 0) {
        return;
    }
    m->owner = 0;
    m->recursion = 0;

    atomic_store(&m->lock, 1);

    int err = futex(&m->lock, FUTEX_WAKE, 1, NULL, NULL, 0);
    if (err == -1) {
        perror("futex WAKE");
        abort();
    }
}
