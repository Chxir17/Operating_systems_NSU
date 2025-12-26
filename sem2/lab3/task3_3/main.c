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

/* ======================= CLIENT HANDLER ======================= */

void *client_handler(void *args) {
    ThreadArgs *thread_args = args;
    int client_socket = thread_args->client_socket;
    Map *cache = thread_args->cache;
    sem_t *sem = thread_args->sem;
    free(thread_args);

    Cache *cache_node = NULL;
    Request *request = read_header(client_socket);
    if (!request || !request->search_path) {
        close(client_socket);
        sem_post(sem);
        return NULL;
    }

    /* ---------- FIND / CREATE CACHE ---------- */

    pthread_mutex_lock(&cache->mutex);
    cache_node = map_find_by_url(cache, request->search_path);

    if (!cache_node) {
        cache_node = map_add(cache, request->search_path);
        cache_node->state = CACHE_LOADING;
        pthread_mutex_unlock(&cache->mutex);
        goto downloader;   // ⬅️ ЭТОТ поток качает
    }

    pthread_mutex_unlock(&cache->mutex);

    /* ======================= READER ======================= */

    Node *cur = NULL;

    pthread_mutex_lock(&cache_node->mutex);
    while (1) {
        if (!cur)
            cur = cache_node->response->first;
        else
            cur = cur->next;

        while (!cur && cache_node->state == CACHE_LOADING) {
            pthread_cond_wait(&cache_node->cond, &cache_node->mutex);
            cur = cache_node->response->first;
            if (cache_node->state == CACHE_ERROR) {
                pthread_mutex_unlock(&cache_node->mutex);
                goto cleanup;
            }
        }

        if (!cur && cache_node->state == CACHE_READY)
            break;

        pthread_mutex_unlock(&cache_node->mutex);

        if (send_to_client(client_socket, cur->value, 0, cur->size) == -1)
            goto cleanup;

        pthread_mutex_lock(&cache_node->mutex);
    }
    pthread_mutex_unlock(&cache_node->mutex);

    goto cleanup;

    /* ======================= DOWNLOADER ======================= */

downloader: {
    int target_socket = http_connect(request);
    if (target_socket == -1)
        goto error;

    if (request_send(target_socket, request) == -1) {
        close(target_socket);
        goto error;
    }

    long len;
    int bufsize = BUFFER_SIZE;

    /* ---- HEADERS ---- */
    while (1) {
        char *line = read_line(target_socket, &len);
        if (!line) break;

        pthread_mutex_lock(&cache_node->mutex);
        list_add(cache_node->response, line, len);
        pthread_cond_broadcast(&cache_node->cond);
        pthread_mutex_unlock(&cache_node->mutex);

        send_to_client(client_socket, line, 0, len);

        if (strstr(line, "Content-Length: "))
            bufsize = atoi(line + strlen("Content-Length: "));

        if (!strcmp(line, "\r\n")) {
            free(line);
            break;
        }
        free(line);
    }

    /* ---- BODY ---- */
    int aborted = 0;

    while (1) {
        char buf[BUFFER_SIZE];
        ssize_t n = recv(target_socket, buf, BUFFER_SIZE, 0);

        if (n <= 0)
            break;

        pthread_mutex_lock(&cache_node->mutex);
        list_add(cache_node->response, buf, n);
        pthread_cond_broadcast(&cache_node->cond);
        pthread_mutex_unlock(&cache_node->mutex);

        if (send_to_client(client_socket, buf, 0, n) == -1) {
            aborted = 1;
            break;
        }
    }

    pthread_mutex_lock(&cache_node->mutex);

    if (aborted) {
        cache_node->state = CACHE_ERROR;
        // optionally: очистить cache_node->response
    } else {
        cache_node->state = CACHE_READY;
    }

    pthread_cond_broadcast(&cache_node->cond);
    pthread_mutex_unlock(&cache_node->mutex);



    close(target_socket);

    pthread_mutex_lock(&cache_node->mutex);
    cache_node->state = CACHE_READY;
    pthread_cond_broadcast(&cache_node->cond);
    pthread_mutex_unlock(&cache_node->mutex);

    goto cleanup;
}

error:
    pthread_mutex_lock(&cache_node->mutex);
    cache_node->state = CACHE_ERROR;
    pthread_cond_broadcast(&cache_node->cond);
    pthread_mutex_unlock(&cache_node->mutex);

cleanup:
    request_destroy(request);
    close(client_socket);
    sem_post(sem);
    return NULL;
}

/* ======================= SERVER ======================= */

void *start_proxy_server(void *arg) {
    int server_socket;
    init_server_socket(&server_socket, PORT, MAX_CLIENTS);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    Map* cache = map_init();

    sem_t sem;
    sem_init(&sem, 0, MAX_THREADS);

    while (1) {
        int client_socket = accept(server_socket,
                                   (struct sockaddr *)&client_addr,
                                   &client_len);
        if (client_socket < 0) continue;

        sem_wait(&sem);

        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->client_socket = client_socket;
        args->cache = cache;
        args->sem = &sem;

        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, args);
        pthread_detach(tid);
    }
}

/* ======================= MAIN ======================= */

int main() {
    signal(SIGPIPE, SIG_IGN);

    pthread_t server;
    pthread_create(&server, NULL, start_proxy_server, NULL);
    pthread_join(server, NULL);
    return 0;
}
