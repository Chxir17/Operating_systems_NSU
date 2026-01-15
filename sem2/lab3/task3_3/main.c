#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <semaphore.h>

#include "handler/handlers.h"
#include "request/request.h"
#include "handler/messages/messages.h"
#include "list/list.h"

#define PORT 8080
#define MAX_THREADS 1000
#define MAX_CLIENTS 1000
#define BUFFER_SIZE 8192

typedef struct ThreadArgs {
    int client_socket;
    Map* cache;
    sem_t *sem;
} ThreadArgs;

void *client_handler(void *args) {
    ThreadArgs *thread_args = args;
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

    //ищем в кэше
    pthread_mutex_lock(&cache->mutex);
    Cache *cache_node = map_find_by_url(cache, request->search_path);

    if (cache_node) {
        // Кэш уже существует — выходим из мьютекса и начинаем streaming
        pthread_mutex_unlock(&cache->mutex);

        printf("Cache entry exists, streaming data...\n");
        Node *current = cache_node->response->first;

        while (1) {
            if (current == NULL) {
                //ждём новых данных или завершения
                if (!list_wait_for_data(cache_node->response, &current)) {
                    break;
                }
                if (current == NULL) {
                    continue;
                }
            }

            // Читаем текущую часть
            pthread_rwlock_rdlock(&current->sync);
            long sent = send(client_socket, current->value, current->size, 0);
            if (sent == -1) {
                pthread_rwlock_unlock(&current->sync);
                printf("Client disconnected during cache streaming\n");
                request_destroy(request);
                close(client_socket);
                free(args);
                sem_post(sem);
                return NULL;
            }
            pthread_rwlock_unlock(&current->sync);

            // Проверяем, завершена ли загрузка
            pthread_mutex_lock(&cache_node->response->mutex);
            int is_complete = cache_node->response->complete;
            Node *next = current->next;
            pthread_mutex_unlock(&cache_node->response->mutex);


            if (next == NULL && is_complete) {
                printf("Streaming from cache completed\n");
                break;
            }
            current = next;
        }
        request_destroy(request);
        close(client_socket);
        free(args);
        sem_post(sem);
        return NULL;
    }

    //новый кэш
    cache_node = map_add(cache, request->search_path);
    pthread_mutex_unlock(&cache->mutex);

    int target_socket = http_connect(request);
    if (target_socket == -1) {
        pthread_mutex_lock(&cache->mutex);
        pthread_mutex_lock(&cache_node->response->mutex);
        cache_node->response->complete = 1;
        pthread_cond_broadcast(&cache_node->response->cond);
        pthread_mutex_unlock(&cache_node->response->mutex);
        pthread_mutex_unlock(&cache->mutex);
    }

    if (request_send(target_socket, request) == -1) {
        close(target_socket);
        pthread_mutex_lock(&cache->mutex);
        pthread_mutex_lock(&cache_node->response->mutex);
        cache_node->response->complete = 1;
        pthread_cond_broadcast(&cache_node->response->cond);
        pthread_mutex_unlock(&cache_node->response->mutex);
        pthread_mutex_unlock(&cache->mutex);
    }

    //заголовки
    int buffer_size = BUFFER_SIZE;
    char buffer[BUFFER_SIZE];
    int client_alive = 1;

    while (1) {
        const long len = read_line(target_socket, buffer, sizeof(buffer));
        if (len <= 0) {
            break;
        }

        list_add(cache_node->response, buffer, len);

        if (client_alive) {
            if (send_to_client(client_socket, buffer, 0, len) == -1) {
                client_alive = 0;
                printf("Client disconnected during header streaming\n");
            }
        }
        if (strncmp(buffer, "Content-Length: ", strlen("Content-Length: ")) == 0) {
            const long content_length = atoll(buffer + strlen("Content-Length: "));
            buffer_size = (content_length < BUFFER_SIZE) ? content_length : BUFFER_SIZE;
        }
        if (buffer[0] == '\r' && buffer[1] == '\n') {
            break;
        }
    }


    //тело
    char body_buf[BUFFER_SIZE];
    long body_len;
    while ((body_len = read_body(target_socket, body_buf, buffer_size)) > 0) {
        list_add(cache_node->response, body_buf, body_len);\
        if (client_alive) {
            if (send_to_client(client_socket, body_buf, 0, body_len) == -1) {
                client_alive = 0;
            }
        }
    }

    close(target_socket);
    pthread_mutex_lock(&cache_node->response->mutex);
    cache_node->response->complete = 1;
    pthread_cond_broadcast(&cache_node->response->cond);
    pthread_mutex_unlock(&cache_node->response->mutex);
    printf("Cache fully loaded\n");
    request_destroy(request);
    close(client_socket);
    free(args);
    sem_post(sem);
    return NULL;
}

void *start_proxy_server(void *arg) {
    int server_socket;
    init_server_socket(&server_socket, PORT, MAX_CLIENTS);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    Map* cache = map_init();

    sem_t sem;
    sem_init(&sem, 0, MAX_THREADS);

    printf("Proxy server started on port %d\n", PORT);

    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        printf("Accepted new connection\n");

        sem_wait(&sem);

        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        if (!args) {
            perror("malloc ThreadArgs");
            sem_post(&sem);
            close(client_socket);
            continue;
        }

        args->client_socket = client_socket;
        args->cache = cache;
        args->sem = &sem;

        pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler, args) != 0) {
            perror("pthread_create");
            free(args);
            close(client_socket);
            sem_post(&sem);
        } else {
            pthread_detach(thread);
        }
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, start_proxy_server, NULL) != 0) {
        perror("pthread_create server");
        return 1;
    }

    pthread_join(server_thread, NULL);
    return 0;
}