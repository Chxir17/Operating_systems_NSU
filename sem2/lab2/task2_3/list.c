#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdatomic.h>


long long increasing_iterations = 0;
long long  decreasing_iterations = 0;
long long equals_iterations = 0;
long long swap_success[3] = {0, 0, 0};
atomic_int stop_flag = 0;

pthread_mutex_t increasing_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t decreasing_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t equal_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t swap_mutex[3] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};

static void random_string(char *buf, const long len){
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    for (size_t i = 0; i < len; ++i) {
        buf[i] = alphabet[rand() % (sizeof(alphabet) - 1)];
    }
    buf[len] = '\0';
}

List *storage_init(const long n) {
    List *l = malloc(sizeof(List));
    if (!l) {
        printf("Cannot allocate memory for list\n");
        abort();
    }
    l->sentinel = malloc(sizeof(Node));
    if (!l->sentinel) {
        free(l);
        printf("Cannot allocate memory for list->sentinel\n");
        abort();
    }
    if (pthread_spin_init(&l->sentinel->sync, 0) != 0) {
        free(l->sentinel);
        free(l);
        printf("Cannot init spin_lock\n");
        abort();
    }
    l->sentinel->value[0] = '\0';
    l->sentinel->next = NULL;
    l->length = n;

    Node *prev = l->sentinel;
    for (long i = 0; i < n; i++) {
        Node *node = malloc(sizeof(Node));
        if (!node) {
            Node *cur = l->sentinel;
            while (cur) {
                Node *next = cur->next;
                pthread_spin_destroy(&cur->sync);
                free(cur);
                cur = next;
            }
            free(l);
            printf("Cannot allocate memory for node\n");
            abort();
        }
        srand(time(NULL));
        const long len = 1 + rand() % (MAX_STR - 1);
        random_string(node->value, len);
        node->next = NULL;
        if (pthread_spin_init(&node->sync, 0) != 0) {
            free(node);
            Node *cur = l->sentinel;
            while (cur) {
                Node *next = cur->next;
                pthread_spin_destroy(&cur->sync);
                free(cur);
                cur = next;
            }
            free(l);
            printf("Cannot init spin_lock for node\n");
            abort();
        }
        prev->next = node;
        prev = node;
    }
    return l;
}

void list_destroy(List *l) {
    if (!l) {
        return;
    }
    Node *cur = l->sentinel;
    while (cur) {
        Node *next = cur->next;
        pthread_spin_destroy(&cur->sync);
        free(cur);
        cur = next;
    }
    free(l);
}

void *increasing_thread(void *arg) {
    List *l = ((struct ThreadArg *)arg)->l;
    while (!stop_flag) {
        Node *prev = l->sentinel;
        pthread_spin_lock(&prev->sync);
        Node *current = prev->next;
        if (!current) {
            pthread_spin_unlock(&prev->sync);
            pthread_mutex_lock(&increasing_mutex);
            increasing_iterations++;
            pthread_mutex_unlock(&increasing_mutex);
            continue;
        }
        pthread_spin_lock(&current->sync);
        int counter = 0;
        while (1) {
            Node *next = current->next;
            if (!next) {
                pthread_spin_unlock(&current->sync);
                pthread_spin_unlock(&prev->sync);
                break;
            }
            pthread_spin_lock(&next->sync);

            const int  current_length = strlen(current->value);
            const int next_length = strlen(next->value);

            pthread_spin_unlock(&next->sync);
            pthread_spin_unlock(&prev->sync);
            if (current_length < next_length) {
                counter++;
            }

            prev = current;
            current = current->next;
            pthread_spin_lock(&current->sync);
        }
        pthread_mutex_lock(&increasing_mutex);
        increasing_iterations++;
        pthread_mutex_unlock(&increasing_mutex);
    }
    return NULL;
}

void *decreasing_thread(void *arg) {
    List *l = ((struct ThreadArg *)arg)->l;
    while (!stop_flag) {
        Node *prev = l->sentinel;
        pthread_spin_lock(&prev->sync);
        Node *current = prev->next;
        if (!current) {
            pthread_spin_unlock(&prev->sync);
            pthread_mutex_lock(&decreasing_mutex);
            decreasing_iterations++;
            pthread_mutex_unlock(&decreasing_mutex);
            continue;
        }
        pthread_spin_lock(&current->sync);
        int counter = 0;
        while (1) {
            Node *next = current->next;
            if (!next){
                pthread_spin_unlock(&current->sync);
                pthread_spin_unlock(&prev->sync);
                break;
            }
            pthread_spin_lock(&next->sync);
            const int current_length = strlen(current->value);
            const int next_length = strlen(next->value);
            pthread_spin_unlock(&next->sync);
            pthread_spin_unlock(&prev->sync);
            if (current_length > next_length){
                counter++;
            }
            prev = current;
            current = current->next;
            pthread_spin_lock(&current->sync);
        }
        pthread_mutex_lock(&decreasing_mutex);
        decreasing_iterations++;
        pthread_mutex_unlock(&decreasing_mutex);
    }
    return NULL;
}

void *equal_thread(void *arg) {
    List *l = ((struct ThreadArg *)arg)->l;
    while (!stop_flag){
        Node *prev = l->sentinel;
        pthread_spin_lock(&prev->sync);
        Node *current = prev->next;
        if (!current){
            pthread_spin_unlock(&prev->sync);
            pthread_mutex_lock(&equal_mutex);
            equals_iterations++;
            pthread_mutex_unlock(&equal_mutex);
            continue;
        }
        pthread_spin_lock(&current->sync);
        int counter = 0;
        while (1)
        {
            Node *next = current->next;
            if (!next){
                pthread_spin_unlock(&current->sync);
                pthread_spin_unlock(&prev->sync);
                break;
            }
            pthread_spin_lock(&next->sync);
            const int current_length = strlen(current->value);
            const int next_length = strlen(next->value);
            pthread_spin_unlock(&next->sync);
            pthread_spin_unlock(&prev->sync);
            if (current_length == next_length) {
                counter++;
            }

            prev = current;
            current = current->next;
            pthread_spin_lock(&current->sync);
        }
        pthread_mutex_lock(&equal_mutex);
        equals_iterations++;
        pthread_mutex_unlock(&equal_mutex);
    }
    return NULL;
}

void *swap_thread(void *arg){
    struct ThreadArg *targ = arg;
    List *l = targ->l;
    int tid = targ->thread_id;
    while (!stop_flag) {
        Node *prev = l->sentinel;
        while (!stop_flag) {
            pthread_spin_lock(&prev->sync);
            Node *current = prev->next;
            if (!current) {
                pthread_spin_unlock(&prev->sync);
                break;
            }
            pthread_spin_lock(&current->sync);
            Node *next = current->next;
            if (!next){
                pthread_spin_unlock(&current->sync);
                pthread_spin_unlock(&prev->sync);
                break;
            }
            if ((rand() & 255) == 0){
                pthread_spin_lock(&nex->sync);
                if (prev->next == current && current->next == next)
                {
                    Node *next_next = next->next;
                    prev->next = next;
                    current->next = next_next;
                    next->next = current;
                }
                pthread_spin_unlock(&next->sync);
                pthread_spin_unlock(&current->sync);
                pthread_spin_unlock(&prev->sync);
            }
            else{
                pthread_spin_unlock(&current->sync);
                pthread_spin_unlock(&prev->sync);
                prev = prev->next;
                if (!prev) {
                    break;
                }
            }
        }
        pthread_mutex_lock(&swap_mutex[tid]);
        swap_success[tid]++;
        pthread_mutex_unlock(&swap_mutex[tid]);
    }
    return NULL;
}

void *monitor_thread(void *arg){
    while (!stop_flag){
        pthread_mutex_lock(&increasing_mutex);
        const long long increasings = increasing_iterations;
        pthread_mutex_unlock(&increasing_mutex);

        pthread_mutex_lock(&decreasing_mutex);
        const long long decreasings = decreasing_iterations;
        pthread_mutex_unlock(&decreasing_mutex);

        pthread_mutex_lock(&equal_mutex);
        const long long equals = equals_iterations;
        pthread_mutex_unlock(&equal_mutex);

        long long swap[3];
        for (int i = 0; i < 3; i++) {
            pthread_mutex_lock(&swap_mutex[i]);
            swap[i] = swap_success[i];
            pthread_mutex_unlock(&swap_mutex[i]);
        }
        printf("ASC=%lld, DESC=%lld, EQ=%lld, SWAP=[%lld, %lld, %lld]\n", increasings, decreasings, equals, swap[0], swap[1], swap[2]);
        sleep(1);
    }
    return NULL;
}