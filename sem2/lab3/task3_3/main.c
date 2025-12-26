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

void *client_handler(void *args) {
    ThreadArgs *thread_args = args; // УБРАЛ const!
    int client_socket = thread_args->client_socket;
    Map *cache = thread_args->cache;
    sem_t *sem = thread_args->sem;

    Request *request = read_header(client_socket);
    if (!request || !request->search_path) {
        close(client_socket);
        free(args);
        sem_post(sem);
        return NULL;
    }

    printf("URL: %s\n", request->search_path);

    pthread_mutex_lock(&cache->mutex);
    Cache *cache_node = map_find_by_url(cache, request->search_path);

    if (cache_node) {
        // Уже есть в кэше — ждём завершения загрузки
        while (!cache_node->is_complete && cache_node->loading) {
            pthread_cond_wait(&cache_node->cond, &cache->mutex);
        }
        pthread_mutex_unlock(&cache->mutex);

        if (cache_node->is_complete) {
            printf("Found in cache, sending...\n");
            Node *current = cache_node->response->first;
            while (current) {
                pthread_rwlock_rdlock(&current->sync);
                if (send_to_client(client_socket, current->value, 0, current->size) == -1) {
                    pthread_rwlock_unlock(&current->sync);
                    goto cleanup;
                }
                pthread_rwlock_unlock(&current->sync);
                current = current->next;
            }
            printf("Sent from cache successfully\n");
            goto cleanup;
        } else {
            // loading = 0 и is_complete = 0 — значит, загрузка прервалась
            pthread_mutex_unlock(&cache->mutex);
            printf("Cache entry broken, reloading...\n");
            // Продолжаем как новый запрос
        }
    }

    // Создаём новый кэш
    cache_node = map_add(cache, request->search_path);
    pthread_mutex_unlock(&cache->mutex);

    // Загружаем данные
    int target_socket = http_connect(request);
    if (target_socket == -1) {
        // Помечаем как неудачный (не завершён)
        pthread_mutex_lock(&cache->mutex);
        cache_node->loading = 0;
        cache_node->is_complete = 0;
        pthread_cond_broadcast(&cache_node->cond);
        pthread_mutex_unlock(&cache->mutex);
        goto cleanup;
    }

    if (request_send(target_socket, request) == -1) {
        close(target_socket);
        pthread_mutex_lock(&cache->mutex);
        cache_node->loading = 0;
        cache_node->is_complete = 0;
        pthread_cond_broadcast(&cache_node->cond);
        pthread_mutex_unlock(&cache->mutex);
        goto cleanup;
    }

    // Отправляем клиенту и кэшируем
    long buffer_length;
    int buffer_size = BUFFER_SIZE;
    Node *current = NULL;
    Node *prev = NULL;
    int client_alive = 1;

    // Заголовки
    while (1) {
        char *buffer = read_line(target_socket, &buffer_length);
        if (!buffer) break;

        current = list_add(cache_node->response, buffer, buffer_length);

        if (client_alive && send_to_client(client_socket, buffer, 0, buffer_length) == -1) {
            client_alive = 0; // перестаём слать клиенту, но продолжаем кэшировать
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

    if (current) pthread_rwlock_wrlock(&current->sync);

    // Тело
    while (1) {
        long body_length;
        char *body = read_body(target_socket, &body_length, buffer_size);
        if (!body) break;

        if (client_alive && send_to_client(client_socket, body, 0, body_length) == -1) {
            client_alive = 0;
        }

        prev = current;
        current = list_add(cache_node->response, body, body_length);
        pthread_rwlock_wrlock(&current->sync);

        if (prev) pthread_rwlock_unlock(&prev->sync);

        free(body);
    }

    if (current) pthread_rwlock_unlock(&current->sync);
    close(target_socket);

    // Помечаем кэш как завершённый
    pthread_mutex_lock(&cache->mutex);
    cache_node->is_complete = 1;
    cache_node->loading = 0;
    pthread_cond_broadcast(&cache_node->cond);
    pthread_mutex_unlock(&cache->mutex);

    printf("Cache fully loaded and sent\n");

cleanup:
    request_destroy(request);
    close(client_socket);
    free(args);
    sem_post(sem);
    return NULL;
}


void *start_proxy_server(void *arg) {
    int server_socket;
    init_server_socket(&server_socket, PORT, MAX_CLIENTS);

    struct sockaddr_in client_addr; //адрес клиента ipv4 + port
    socklen_t client_len = sizeof(client_addr);

    Map* cache = map_init();

    sem_t sem;
    //для установки кол-ва клиентов
    sem_init(&sem, 0, MAX_THREADS);

    while (1) {
        const int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        printf("Accepted a new connection\n");
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

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

int main() {
    //игнорим сигпайп чтобы если используем закрытый пайп сервер не падал с ошибкой
    signal(SIGPIPE, SIG_IGN);
    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, start_proxy_server, NULL) != 0) {
        perror("Error creating server thread");
        abort();
    }
    pthread_join(server_thread, NULL);
    return 0;
}