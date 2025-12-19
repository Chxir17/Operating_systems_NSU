#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

long long increasing_iterations = 0;
long long decreasing_iterations = 0;
long long equals_iterations = 0;
long long swap_success[3] = {0, 0, 0};

atomic_int stop_flag = 0;

pthread_rwlock_t increasing_lock = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t decreasing_lock = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t equal_lock = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t swap_lock[3] = {
    PTHREAD_RWLOCK_INITIALIZER,
    PTHREAD_RWLOCK_INITIALIZER,
    PTHREAD_RWLOCK_INITIALIZER
};

static void random_string(char *buf, long len) {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    for (long i = 0; i < len; i++) {
        buf[i] = alphabet[rand() % (sizeof(alphabet) - 1)];
    }
    buf[len] = '\0';
}

List *list_init(long n) {
    srand(time(NULL));
    List *l = malloc(sizeof(List));
    l->sentinel = malloc(sizeof(Node));

    pthread_rwlock_init(&l->sentinel->sync, NULL);
    l->sentinel->value[0] = '\0';
    l->sentinel->next = NULL;
    l->length = n;

    Node *prev = l->sentinel;
    for (long i = 0; i < n; i++) {
        Node *node = malloc(sizeof(Node));
        pthread_rwlock_init(&node->sync, NULL);

        long len = 1 + rand() % (MAX_STR - 1);
        random_string(node->value, len);

        node->next = NULL;
        prev->next = node;
        prev = node;
    }
    return l;
}

void list_destroy(List *l) {
    Node *cur = l->sentinel;
    while (cur) {
        Node *next = cur->next;
        pthread_rwlock_destroy(&cur->sync);
        free(cur);
        cur = next;
    }
    free(l);
}

void *increasing_thread(void *arg) {
    List *l = ((struct ThreadArg *)arg)->l;

    while (!stop_flag) {
        Node *prev = l->sentinel;
        pthread_rwlock_rdlock(&prev->sync);

        Node *current = prev->next;
        if (!current) {
            pthread_rwlock_unlock(&prev->sync);
            pthread_rwlock_wrlock(&increasing_lock);
            increasing_iterations++;
            pthread_rwlock_unlock(&increasing_lock);
            continue;
        }

        pthread_rwlock_rdlock(&current->sync);
        int counter = 0;

        while (current->next) {
            Node *next = current->next;
            pthread_rwlock_rdlock(&next->sync);

            if (strlen(current->value) < strlen(next->value)) {
                counter++;
            }

            pthread_rwlock_unlock(&prev->sync);
            prev = current;
            current = next;
        }

        pthread_rwlock_unlock(&current->sync);
        pthread_rwlock_unlock(&prev->sync);

        pthread_rwlock_wrlock(&increasing_lock);
        increasing_iterations++;
        pthread_rwlock_unlock(&increasing_lock);
    }
    return NULL;
}

void *decreasing_thread(void *arg) {
    List *l = ((struct ThreadArg *)arg)->l;

    while (!stop_flag) {
        Node *prev = l->sentinel;
        pthread_rwlock_rdlock(&prev->sync);

        Node *current = prev->next;
        if (!current) {
            pthread_rwlock_unlock(&prev->sync);
            pthread_rwlock_wrlock(&decreasing_lock);
            decreasing_iterations++;
            pthread_rwlock_unlock(&decreasing_lock);
            continue;
        }

        pthread_rwlock_rdlock(&current->sync);
        int counter = 0;

        while (current->next) {
            Node *next = current->next;
            pthread_rwlock_rdlock(&next->sync);

            if (strlen(current->value) > strlen(next->value)) {
                counter++;
            }

            pthread_rwlock_unlock(&prev->sync);
            prev = current;
            current = next;
        }

        pthread_rwlock_unlock(&current->sync);
        pthread_rwlock_unlock(&prev->sync);

        pthread_rwlock_wrlock(&decreasing_lock);
        decreasing_iterations++;
        pthread_rwlock_unlock(&decreasing_lock);
    }
    return NULL;
}

void *equal_thread(void *arg) {
    List *l = ((struct ThreadArg *)arg)->l;

    while (!stop_flag) {
        Node *prev = l->sentinel;
        pthread_rwlock_rdlock(&prev->sync);

        Node *current = prev->next;
        if (!current) {
            pthread_rwlock_unlock(&prev->sync);
            pthread_rwlock_wrlock(&equal_lock);
            equals_iterations++;
            pthread_rwlock_unlock(&equal_lock);
            continue;
        }

        pthread_rwlock_rdlock(&current->sync);
        int counter = 0;

        while (current->next) {
            Node *next = current->next;
            pthread_rwlock_rdlock(&next->sync);

            if (strlen(current->value) == strlen(next->value)) {
                counter++;
            }

            pthread_rwlock_unlock(&prev->sync);
            prev = current;
            current = next;
        }

        pthread_rwlock_unlock(&current->sync);
        pthread_rwlock_unlock(&prev->sync);

        pthread_rwlock_wrlock(&equal_lock);
        equals_iterations++;
        pthread_rwlock_unlock(&equal_lock);
    }
    return NULL;
}

void *swap_thread(void *arg) {
    struct ThreadArg *targ = arg;
    List *l = targ->l;
    int tid = targ->thread_id;

    while (!stop_flag) {
        Node *prev = l->sentinel;

        while (!stop_flag) {
            pthread_rwlock_wrlock(&prev->sync);

            Node *current = prev->next;
            if (!current) {
                pthread_rwlock_unlock(&prev->sync);
                break;
            }

            pthread_rwlock_wrlock(&current->sync);
            Node *next = current->next;
            if (!next) {
                pthread_rwlock_unlock(&current->sync);
                pthread_rwlock_unlock(&prev->sync);
                break;
            }

            if ((rand() & 255) == 0) {
                pthread_rwlock_wrlock(&next->sync);

                if (prev->next == current && current->next == next) {
                    Node *nn = next->next;
                    prev->next = next;
                    current->next = nn;
                    next->next = current;
                }

                pthread_rwlock_unlock(&next->sync);
                pthread_rwlock_unlock(&current->sync);
                pthread_rwlock_unlock(&prev->sync);
            } else {
                pthread_rwlock_unlock(&current->sync);
                pthread_rwlock_unlock(&prev->sync);
                prev = prev->next;
                if (!prev)
                    break;
            }
        }

        pthread_rwlock_wrlock(&swap_lock[tid]);
        swap_success[tid]++;
        pthread_rwlock_unlock(&swap_lock[tid]);
    }
    return NULL;
}

void *monitor_thread(void *arg) {
    while (!stop_flag) {
        pthread_rwlock_rdlock(&increasing_lock);
        long long inc = increasing_iterations;
        pthread_rwlock_unlock(&increasing_lock);

        pthread_rwlock_rdlock(&decreasing_lock);
        long long dec = decreasing_iterations;
        pthread_rwlock_unlock(&decreasing_lock);

        pthread_rwlock_rdlock(&equal_lock);
        long long eq = equals_iterations;
        pthread_rwlock_unlock(&equal_lock);

        long long sw[3];
        for (int i = 0; i < 3; i++) {
            pthread_rwlock_rdlock(&swap_lock[i]);
            sw[i] = swap_success[i];
            pthread_rwlock_unlock(&swap_lock[i]);
        }

        printf("Increases=%lld, Decreases=%lld, Equals=%lld, Swaps=[%lld, %lld, %lld]\n",
               inc, dec, eq, sw[0], sw[1], sw[2]);
        sleep(1);
    }
    return NULL;
}
