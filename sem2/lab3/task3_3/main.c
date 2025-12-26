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
    ThreadArgs *thread_args = args;
    int client_socket = thread_args->client_socket;
    Map *cache = thread_args->cache;
    sem_t *sem = thread_args->sem;

    Request *request = read_header(client_socket);
    if (!request || !request->search_path) {
        close(client_socket);
        sem_post(sem);
        return NULL;
    }

    Cache* cache_node = map_find_by_url(cache, request->search_path);
    if (!cache_node) {
        cache_node = map_add(cache, request->search_path);
    }

    List* cachedResponse = cache_node->response;

    int target_socket = http_connect(request);
    if (target_socket == -1) {
        close(client_socket);
        request_destroy(request);
        sem_post(sem);
        return NULL;
    }

    // Загружаем и отправляем по мере поступления
    Node* current = NULL;
    char buffer[BUFFER_SIZE];
    long bytes_read;

    while ((bytes_read = recv(target_socket, buffer, sizeof(buffer), 0)) > 0) {
        // добавляем в кеш
        current = list_add(cachedResponse, buffer, bytes_read);

        pthread_rwlock_wrlock(&current->sync);
        current->ready = 1;
        pthread_cond_broadcast(&current->cond);
        pthread_rwlock_unlock(&current->sync);

        // сразу отправляем клиенту
        if (send_to_client(client_socket, current->value, 0, current->size) == -1) {
            break;
        }
    }

    close(target_socket);

    // ждем, пока клиент прочитает оставшиеся данные
    current = cachedResponse->first;
    while (current) {
        pthread_rwlock_wrlock(&current->sync);
        while (!current->ready) {
            pthread_cond_wait(&current->cond, &current->sync);
        }
        pthread_rwlock_unlock(&current->sync);
        current = current->next;
    }

    request_destroy(request);
    close(client_socket);
    sem_post(sem);
    free(thread_args);
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