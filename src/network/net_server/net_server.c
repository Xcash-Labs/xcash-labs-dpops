#include "net_server.h"

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

  while (atomic_load(&server_running)) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int* client_socket = malloc(sizeof(int));
    if (!client_socket) {
      perror("malloc");
      continue;
    }

    *client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (*client_socket < 0) {
      free(client_socket);

      if (!atomic_load(&server_running)) {
        // Shutdown has been triggered, exit cleanly
        break;
      }

      perror("accept");  // Only log if not shutting down
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

  server_client_t client = { .socket_fd = client_socket };

  // Set receive timeout (5 seconds)
  struct timeval recv_timeout = {RECEIVE_TIMEOUT_SEC, 0};
  setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

  // Get client IP
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  if (getpeername(client_socket, (struct sockaddr*)&addr, &addr_len) == 0) {
    inet_ntop(AF_INET, &addr.sin_addr, client.client_ip, sizeof(client.client_ip));
  } else {
    strncpy(client.client_ip, "unknown", sizeof(client.client_ip));
  }

  char buffer[SMALL_BUFFER_SIZE];

  while (1) {
    ssize_t bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        WARNING_PRINT("recv() timed out from %s", client.client_ip);
      }
      break;
    }

    buffer[bytes] = '\0';

    unsigned char* decompressed = NULL;
    size_t decompressed_len = 0;

    if (!decompress_gzip_with_prefix((const unsigned char*)buffer, (size_t)bytes, &decompressed, &decompressed_len)) {
      WARNING_PRINT("Failed to decompress message from %s", client.client_ip);
      continue;
    }

    DEBUG_PRINT("[TCP] Message from %s: %.*s", client.client_ip, (int)decompressed_len, decompressed);
    handle_srv_message((char*)decompressed, decompressed_len, &client);
    free(decompressed);
  }

  close(client_socket);
  return NULL;
}

int send_data(server_client_t* client, const unsigned char* data, size_t length) {
  if (!client) {
    ERROR_PRINT("send_data failed: client is NULL");
    return XCASH_ERROR;
  }

  if (client->socket_fd < 0) {
    ERROR_PRINT("send_data failed: invalid socket_fd (%d) for client %s", client->socket_fd, client->client_ip);
    return XCASH_ERROR;
  }

  ssize_t sent = send(client->socket_fd, data, length, MSG_NOSIGNAL);

  if (sent < 0) {
    ERROR_PRINT("Failed to send data to %s. Message: %.100s", client->client_ip, data);
    return XCASH_ERROR;
  }

  DEBUG_PRINT("Sent %zd bytes to %s. Message: %.100s", sent, client->client_ip, data);
  return XCASH_OK;
}

void stop_tcp_server(void) {
  if (!atomic_load(&server_running)) return;

  printf("Stopping TCP server...\n");

  atomic_store(&server_running, false);

  // Close the listening socket to unblock accept()
  if (server_fd >= 0) {
    close(server_fd);
    server_fd = -1;
  }

  pthread_join(server_thread, NULL);

  printf("TCP server stopped.\n");
}