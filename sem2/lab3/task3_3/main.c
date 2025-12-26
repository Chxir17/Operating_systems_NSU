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

    printf("URL: %s\n", request->search_path);
    Cache* cache_node = map_find_by_url(cache, request->search_path);
    if (!cache_node) {
        cache_node = map_add(cache, request->search_path);
    }

    List* cachedResponse = cache_node->response;

    // Соединяемся с сервером и начинаем загрузку в отдельный поток
    int target_socket = http_connect(request);
    if (target_socket != -1) {
        Node* last_node = NULL;
        while (1) {
            long body_length;
            char* body = read_body(target_socket, &body_length, BUFFER_SIZE);
            if (!body) break;

            Node* node = list_add(cachedResponse, body, body_length);
            free(body);

            pthread_rwlock_wrlock(&node->sync);
            node->ready = 1;
            pthread_cond_broadcast(&node->cond); // оповестить клиентов
            pthread_rwlock_unlock(&node->sync);

            last_node = node;
        }
        close(target_socket);
    }

    // Клиент читает данные из кеша
    Node* current = cachedResponse->first;
    while (current) {
        pthread_rwlock_wrlock(&current->sync);
        while (!current->ready) {
            pthread_cond_wait(&current->cond, &current->sync);
        }
        pthread_rwlock_unlock(&current->sync);

        if (send_to_client(client_socket, current->value, 0, current->size) == -1) {
            break;
        }
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