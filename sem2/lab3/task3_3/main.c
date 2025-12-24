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

#define PORT 80
#define MAX_THREADS 100
#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096

typedef struct ThreadArgs {
    int client_socket;
    Map* cache;
    sem_t *sem;
} ThreadArgs;

void *client_handler(void *args) {
    const ThreadArgs *thread_args = args;
    const int client_socket = thread_args->client_socket;
    Map *cache = thread_args->cache;
    sem_t *sem = thread_args->sem;

    Cache* cache_node = NULL;
    List* cachedResponse = NULL;

    Request *request = read_header(client_socket);

    if (request == NULL) {
        printf("Failed to parse the header\n");

        close(client_socket);
        return NULL;
    }
    if (request->search_path == NULL) {
        close(client_socket);

        return NULL;
    }
    printf("URL: %s\n", request->search_path);

    pthread_mutex_lock(&cache->mutex);
    cache_node = map_find_by_url(cache, request->search_path);

    int data = 0;
    if (cache_node == NULL) {
        int buffer_size = BUFFER_SIZE;
        // В кеше не нашлось данных
        printf("There is not in cache\n");
        int target_socket = http_connect(request);
        if (target_socket == -1) {
            printf("Failed to connect to host\n");

            http_request_destroy(request);
            close(client_socket);
            pthread_mutex_unlock(&cache->mutex);

            return NULL;
        }
        if (http_request_send(target_socket, request) == -1) {

            http_request_destroy(request);
            close(client_socket);
            pthread_mutex_unlock(&cache->mutex);

            return NULL;
        }

        printf("Start to retrieve the response header\n");
        cache_node = map_add(cache, request->search_path);

        cachedResponse = cache_node->response;
        long buffer_length;
        Node* current = NULL;
        Node* prev = NULL;
        //int cont_lenght_size = strlen("Content-Length: ");
        while (1) {
            buffer_length = 0;
            char *buffer = read_line(target_socket, &buffer_length);
            //printf("STRLEN() = %d\n", line_length);
            data++;
            //  printf("line: %s\n", line);
            current = list_add(cachedResponse, buffer, buffer_length);
            //printf("storage_add: %s\n", current->value);


            int err = send_to_client(client_socket, buffer, 0, buffer_length);
            if (err == -1) {
                printf("Send to client headers end with ERROR\n");
                free(buffer);
                pthread_mutex_unlock(&cache->mutex);

                return NULL;
            }

            if (strstr(buffer, "Content-Length: ")) {
                long content_length = atoll(buffer + strlen("Content-Length: "));
                buffer_size = (content_length < BUFFER_SIZE) ? content_length : BUFFER_SIZE;
            }

            if (buffer[0] == '\r' && buffer[1] == '\n') {
                printf("get the end of the HTTP response header\n");
                free(buffer);
                break;
            }
            free(buffer);
        }
        pthread_mutex_unlock(&cache->mutex);  // разблокировали хедеры

        pthread_rwlock_wrlock(&current->sync);
        printf("Start to send BODY with buffer %d bytes in package\n", buffer_size);
        while (1) {
            prev = current;
            long body_length;
            char *body = read_body(target_socket, &body_length, buffer_size);
            if (body == NULL) {
                break;
            }
            int err = send_to_client(client_socket, body, 0, body_length);
            if (err == -1) {
                printf("Send to client body ended with ERROR\n");
                free(body);

                pthread_rwlock_unlock(&prev->sync);
                return NULL;
            }
            data++;
            current = list_add(cachedResponse, body, body_length);
            pthread_rwlock_wrlock(&current->sync);
            pthread_rwlock_unlock(&prev->sync);
            free(body);
            //sleep(2);
        }
        pthread_rwlock_unlock(&prev->sync);

        printf("Send to client body was success\n");

        close(target_socket);
    } else {
        pthread_mutex_unlock(&cache->mutex);
        // Нашли в кеше
        printf("Found in cache, start to send it\n");
        cachedResponse = cache_node->response;

        Node* current = cachedResponse->first;
        data = 0;
        while (current != NULL) {
            pthread_rwlock_rdlock(&current->sync);
            data++;
            int err = send_to_client(client_socket, current->value, 0, current->size);
            if (err == -1) {
                printf("Send to client body ended with ERROR\n");
                pthread_rwlock_unlock(&current->sync);
                return NULL;
            }
            //printf("from cache: %s\n", current->value);
            //sleep(1);

            pthread_rwlock_unlock(&current->sync);
            current = current->next;
        }

        printf("Send to client data from cache success\n");
    }

    sem_post(sem);

    //printf("K = %d\n", k);
    http_request_destroy(request);
    close(client_socket);
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