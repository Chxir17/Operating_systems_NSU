//
// Created by Ilya Zyryanov on 19.12.2025.
//

#ifndef OS_MY_SPINLOCK_H
#define OS_MY_SPINLOCK_H

#ifndef MY_SPINLOCK_H
#define MY_SPINLOCK_H

typedef volatile int my_spinlock_t;

void my_spinlock_init(my_spinlock_t *s);
void my_spinlock_lock(my_spinlock_t *s);
void my_spinlock_unlock(my_spinlock_t *s);

#endif

#endif //OS_MY_SPINLOCK_H