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
#define BUFFER_SIZE 4096

typedef struct ThreadArgs {
    int client_socket;
    Map* cache;
    sem_t *sem;
} ThreadArgs;

static int wait_for_more_data_or_complete(List* list, Node** current) {
    pthread_mutex_lock(&list->mutex);
    while (*current == list->last && !list->complete) {
        if (pthread_cond_wait(&list->cond, &list->mutex) != 0) {
            pthread_mutex_unlock(&list->mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&list->mutex);
    return 1;
}

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
                if (!wait_for_more_data_or_complete(cache_node->response, &current)) {
                    break;
                }
                if (current == NULL) continue;
            }

            // Читаем текущую часть
            pthread_rwlock_rdlock(&current->sync);
            ssize_t sent = send(client_socket, current->value, current->size, 0);
            if (sent == -1) {
                pthread_rwlock_unlock(&current->sync);
                printf("Client disconnected during cache streaming\n");
                goto cleanup;
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
        goto cleanup;
    }

    //новый кэш
    cache_node = map_add(cache, request->search_path);
    pthread_mutex_unlock(&cache->mutex);

    int target_socket = http_connect(request);
    if (target_socket == -1) {
        goto mark_failed_and_cleanup;
    }

    if (request_send(target_socket, request) == -1) {
        close(target_socket);
        goto mark_failed_and_cleanup;
    }

    //заголовки
    long buffer_length;
    int buffer_size = BUFFER_SIZE;
    Node *current = NULL;
    Node *prev = NULL;
    int client_alive = 1;

    while (1) {
        char *buffer = read_line(target_socket, &buffer_length);
        if (!buffer) break;
        current = list_add(cache_node->response, buffer, buffer_length);
        //если жив
        if (client_alive) {
            if (send_to_client(client_socket, buffer, 0, buffer_length) == -1) {
                client_alive = 0;
                printf("Client disconnected during header streaming\n");
            }
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

    if (current) {
        pthread_rwlock_wrlock(&current->sync);
    }

    //тело
    while (1) {
        long body_length;
        char *body = read_body(target_socket, &body_length, buffer_size);
        if (!body) break;
        prev = current;
        current = list_add(cache_node->response, body, body_length);
        pthread_rwlock_wrlock(&current->sync);

        if (prev) {
            pthread_rwlock_unlock(&prev->sync);
        }

        //если жив
        if (client_alive) {
            if (send_to_client(client_socket, body, 0, body_length) == -1) {
                client_alive = 0;
                printf("Client disconnected during body streaming\n");
            }
        }

        free(body);
    }
    if (current) {
        pthread_rwlock_unlock(&current->sync);
    }

    close(target_socket);
    pthread_mutex_lock(&cache_node->response->mutex);
    cache_node->response->complete = 1;
    pthread_cond_broadcast(&cache_node->response->cond);
    pthread_mutex_unlock(&cache_node->response->mutex);
    printf("Cache fully loaded\n");
    goto cleanup;

mark_failed_and_cleanup:
    // Помечаем как неудачный (но не удаляем — чтобы не создавать повторно)
    pthread_mutex_lock(&cache->mutex);
    pthread_mutex_lock(&cache_node->response->mutex);
    cache_node->response->complete = 1;
    pthread_cond_broadcast(&cache_node->response->cond);
    pthread_mutex_unlock(&cache_node->response->mutex);
    pthread_mutex_unlock(&cache->mutex);

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