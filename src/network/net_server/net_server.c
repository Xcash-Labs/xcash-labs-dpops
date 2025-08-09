#include "net_server.h"

int server_fd = -1;
sem_t client_slots;

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

  struct linger so_linger = {1, 0};
  setsockopt(server_fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));

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

  sem_init(&client_slots, 0, MAX_ACTIVE_CLIENTS);

  if (pthread_create(&server_thread, NULL, server_thread_loop, NULL) != 0) {
    perror("pthread_create");
    close(server_fd);
    return 0;
  }

  printf("Server listening on port %d\n", port);
  return 1;
}

void* server_thread_loop(void* arg) {
  (void)arg;

  while (atomic_load(&server_running)) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_client_t* client = malloc(sizeof(server_client_t));
    if (!client) {
      perror("malloc");
      continue;
    }

    client->socket_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client->socket_fd < 0) {
      free(client);

      if (!atomic_load(&server_running)) {
        break;
      }

      perror("accept");
      continue;
    }

    if (inet_ntop(AF_INET, &client_addr.sin_addr, client->client_ip, sizeof(client->client_ip)) == NULL) {
      strncpy(client->client_ip, "unknown", sizeof(client->client_ip));
      client->client_ip[sizeof(client->client_ip) - 1] = '\0';  // Ensure null-termination
    }

    sem_wait(&client_slots);

    pthread_t client_thread;
    if (pthread_create(&client_thread, NULL, handle_client, client) != 0) {
      perror("pthread_create");
      close(client->socket_fd);
      free(client);
      sem_post(&client_slots);
      continue;
    }

    pthread_detach(client_thread);
  }

  return NULL;
}

void* handle_client(void* arg) {
  server_client_t* client = (server_client_t*)arg;

  struct timeval recv_timeout = {RECEIVE_TIMEOUT_SEC, 0};
  setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

  char buffer[BUFFER_SIZE];

  while (1) {
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes = recv(client->socket_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes < 0 && errno == EINTR) continue;
    if (bytes <= 0) break;

    buffer[bytes] = '\0';

    if (strncmp(buffer, "GET ", 4) == 0 || strncmp(buffer, "POST", 4) == 0 || strncmp(buffer, "HEAD", 4) == 0) {
      WARNING_PRINT("Rejected HTTP request from %s", client->client_ip);
      break;
    }

    unsigned char* decompressed = NULL;
    size_t decompressed_len = 0;
    if (!decompress_gzip_with_prefix((const unsigned char*)buffer, (size_t)bytes, &decompressed, &decompressed_len)) {
      WARNING_PRINT("Failed to decompress message from %s", client->client_ip);
      continue;
    }

    DEBUG_PRINT("[TCP] Message from %s: %.*s\n", client->client_ip, (int)decompressed_len, decompressed);
    handle_srv_message((char*)decompressed, decompressed_len, client);
    free(decompressed);
  }

  close(client->socket_fd);
  free(client);
  sem_post(&client_slots);
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

  if (server_fd >= 0) {
    shutdown(server_fd, SHUT_RDWR);
    close(server_fd);
    server_fd = -1;
  }

  pthread_join(server_thread, NULL);

  sem_destroy(&client_slots);

  printf("TCP server stopped.\n");
}

int send_message_to_ip_or_hostname(const char* host_or_ip, int port, const char* message) {
  if (!host_or_ip || !message) {
    ERROR_PRINT("Invalid arguments to send_message_to_ip_or_hostname");
    return XCASH_ERROR;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  // Try interpreting as raw IP first
  if (inet_pton(AF_INET, host_or_ip, &addr.sin_addr) != 1) {
    // Not a raw IP — try resolving hostname
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host_or_ip, port_str, &hints, &res) != 0) {
      ERROR_PRINT("DNS resolution failed for host: %s", host_or_ip);
      return XCASH_ERROR;
    }

    struct sockaddr_in* resolved = (struct sockaddr_in*)res->ai_addr;
    addr.sin_addr = resolved->sin_addr;
    freeaddrinfo(res);
  }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    return XCASH_ERROR;
  }

  struct linger so_linger = {1, 0};  // Force immediate close
  setsockopt(sock, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));

  struct timeval timeout = {CONNECT_TIMEOUT_SEC, 0};
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("connect");
    close(sock);
    return XCASH_ERROR;
  }

  unsigned char* compressed = NULL;
  size_t compressed_len = 0;
  size_t msg_len = strlen(message) + 1;
  if (!compress_gzip_with_prefix((const unsigned char*)message, msg_len, &compressed, &compressed_len)) {
    ERROR_PRINT("Compression failed");
    close(sock);
    return XCASH_ERROR;
  }

  ssize_t sent = send(sock, compressed, compressed_len, MSG_NOSIGNAL);
  free(compressed);

  if (sent < 0) {
    perror("send");
    close(sock);
    return XCASH_ERROR;
  }

  INFO_PRINT("Sent message to %s:%d", host_or_ip, port);
  close(sock);
  return XCASH_OK;
}