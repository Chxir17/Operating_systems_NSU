//
// Created by Ilya Zyryanov on 19.12.2025.
//

#ifndef OS_MY_MUTEX_H
#define OS_MY_MUTEX_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdatomic.h>
#include <linux/futex.h>
#include <sys/syscall.h>


static inline pid_t get_tid(void) {
	return (pid_t)syscall(SYS_gettid);
}

typedef enum {
	MY_MUTEX_NORMAL,
	MY_MUTEX_RECURSIVE,
	MY_MUTEX_ERRORCHECK
} my_mutex_type_t;

typedef struct {
	atomic_int lock;     // 1 = unlocked, 0 = locked
	pid_t owner;         // TID владельца
	int recursion;       // глубина рекурсии
	my_mutex_type_t type;
} my_mutex_t;


void my_mutex_init(my_mutex_t* m, my_mutex_type_t type);

int futex(int* uaddr, int futex_op, int val,
	const struct timespec* timeout, int* uaddr2, int val3);

void my_mutex_lock(my_mutex_t* m);

void my_mutex_unlock(my_mutex_t* m);

#endif //OS_MY_MUTEX_H