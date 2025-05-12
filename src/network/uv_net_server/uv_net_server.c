#include "uv_net_server.h"

int server_fd = -1;

// Start the TCP server
int start_tcp_server(int port) {
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 0;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 0;
    }

    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        close(server_fd);
        return 0;
    }

    if (pthread_create(&server_thread, NULL, server_thread_loop, NULL) != 0) {
        perror("pthread_create");
        close(server_fd);
        return 0;
    }

    printf("Server listening on port %d\n", port);
    return 1;
}

// Accept and spawn client threads
void* server_thread_loop(void* arg) {
    (void)arg;

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int* client_socket = malloc(sizeof(int));
        if (!client_socket) {
            perror("malloc");
            continue;
        }

        *client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (*client_socket < 0) {
            perror("accept");
            free(client_socket);
            continue;
        }

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, client_socket) != 0) {
            perror("pthread_create");
            close(*client_socket);
            free(client_socket);
            continue;
        }

        pthread_detach(client_thread);
    }

    return NULL;
}

void* handle_client(void* client_socket_ptr) {
    int client_socket = *(int*)client_socket_ptr;
    free(client_socket_ptr);

    char buffer[SMALL_BUFFER_SIZE];

    while (1) {
        ssize_t bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            break;  // Client disconnected or error
        }

        buffer[bytes] = '\0';
        printf("[TCP] Messaged Received: %s\n", buffer);
    }

    close(client_socket);
    return NULL;
}