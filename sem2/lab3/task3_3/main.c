#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <semaphore.h>

#include "handler/handlers.h"
#include "proxy/proxy.h"
#include "handler/messages/messages.h"
#include "list/list.h"

#define PORT 8080
#define MAX_THREADS 100
#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096

typedef struct ThreadArgs {
    int client_socket;
    Map* cache;
    sem_t *sem;
} ThreadArgs;

// ---------------- Streamed cache for multiple clients ----------------
void send_from_cache(Cache* cache, int client_socket) {
    Node* cur = NULL;

    pthread_mutex_lock(&cache->mutex);
    cur = cache->response->first;

    while (1) {
        // ждём появления новых данных
        while (cur == NULL && !cache->completed) {
            pthread_cond_wait(&cache->cond, &cache->mutex);
            cur = cache->response->first;
        }

        while (cur) {
            pthread_mutex_unlock(&cache->mutex);

            if (send_to_client(client_socket, cur->value, 0, cur->size) == -1)
                return;

            pthread_mutex_lock(&cache->mutex);
            cur = cur->next;
        }

        if (cache->completed)
            break;
    }

    pthread_mutex_unlock(&cache->mutex);
}

// ---------------- Client handler ----------------
void *client_handler(void *args) {
    ThreadArgs *thread_args = (ThreadArgs*)args;
    int client_socket = thread_args->client_socket;
    Map *cache = thread_args->cache;
    sem_t *sem = thread_args->sem;

    Cache *cache_node = NULL;

    Request *request = read_header(client_socket);
    if (!request || !request->search_path) {
        close(client_socket);
        sem_post(sem);
        free(thread_args);
        return NULL;
    }

    printf("URL: %s\n", request->search_path);

    pthread_mutex_lock(&cache->mutex);
    cache_node = map_find_by_url(cache, request->search_path);
    if (cache_node) {
        pthread_mutex_unlock(&cache->mutex);
        printf("Found in cache, streaming to client\n");
        send_from_cache(cache_node, client_socket);
        goto cleanup;
    }

    // Если нет — создаём новый cache node
    cache_node = map_add(cache, request->search_path);
    pthread_mutex_unlock(&cache->mutex);

    int target_socket = http_connect(request);
    if (target_socket == -1) goto cleanup;

    if (request_send(target_socket, request) == -1) {
        close(target_socket);
        goto cleanup;
    }

    long buffer_length;
    int buffer_size = BUFFER_SIZE;
    Node *current = NULL;
    Node *prev = NULL;

    // ---------------- Read headers ----------------
    while (1) {
        char *buffer = read_line(target_socket, &buffer_length);
        if (!buffer) break;

        current = list_add(cache_node->response, buffer, buffer_length);

        // Уведомляем ожидающих клиентов
        pthread_mutex_lock(&cache_node->mutex);
        pthread_cond_broadcast(&cache_node->cond);
        pthread_mutex_unlock(&cache_node->mutex);

        if (send_to_client(client_socket, buffer, 0, buffer_length) == -1) {
            free(buffer);
            goto cleanup;
        }

        if (strstr(buffer, "Content-Length: ")) {
            long cl = atoll(buffer + strlen("Content-Length: "));
            buffer_size = (cl < BUFFER_SIZE) ? cl : BUFFER_SIZE;
        }

        if (buffer[0] == '\r' && buffer[1] == '\n') {
            free(buffer);
            break;
        }

        free(buffer);
    }

    pthread_rwlock_wrlock(&current->sync);

    // ---------------- Read body ----------------
    while (1) {
        long body_length;
        char *body = read_body(target_socket, &body_length, buffer_size);
        if (!body) break;

        prev = current;
        current = list_add(cache_node->response, body, body_length);

        pthread_rwlock_wrlock(&current->sync);
        pthread_rwlock_unlock(&prev->sync);

        // Уведомляем всех клиентов, что появились новые данные
        pthread_mutex_lock(&cache_node->mutex);
        pthread_cond_broadcast(&cache_node->cond);
        pthread_mutex_unlock(&cache_node->mutex);

        if (send_to_client(client_socket, body, 0, body_length) == -1) {
            free(body);
            pthread_rwlock_unlock(&current->sync);
            goto cleanup;
        }

        free(body);
    }

    pthread_rwlock_unlock(&current->sync);
    close(target_socket);

    // ---------------- Signal completion ----------------
    pthread_mutex_lock(&cache_node->mutex);
    cache_node->completed = 1;
    pthread_cond_broadcast(&cache_node->cond);
    pthread_mutex_unlock(&cache_node->mutex);

    printf("Finished sending & caching\n");

cleanup:
    request_destroy(request);
    close(client_socket);
    sem_post(sem);
    free(thread_args);
    return NULL;
}

// ---------------- Server ----------------
void *start_proxy_server(void *arg) {
    int server_socket;
    init_server_socket(&server_socket, PORT, MAX_CLIENTS);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    Map* cache = map_init();

    sem_t sem;
    sem_init(&sem, 0, MAX_THREADS);

    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }
        printf("Accepted new connection\n");

        sem_wait(&sem);

        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->client_socket = client_socket;
        args->cache = cache;
        args->sem = &sem;

        pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler, args) != 0) {
            perror("Error creating thread");
            sem_post(&sem);
            free(args);
            close(client_socket);
        } else {
            pthread_detach(thread);
        }
    }
}

// ---------------- Main ----------------
int main() {
    signal(SIGPIPE, SIG_IGN);

    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, start_proxy_server, NULL) != 0) {
        perror("Error creating server thread");
        abort();
    }

    pthread_join(server_thread, NULL);
    return 0;
}
