#include "net_multi.h"

response_t** send_multi_request(const char** hosts, int port, const char* message) {
  int total_hosts = 0;
  while (hosts[total_hosts]) total_hosts++;

  response_t** responses = calloc(total_hosts + 1, sizeof(response_t*));
  if (!responses) {
    perror("calloc responses");
    return NULL;
  }

  for (int i = 0; i < total_hosts; i++) {
    const char* host = hosts[i];
    response_t* response = calloc(1, sizeof(response_t));
    if (!response) {
      response_t* dummy = calloc(1, sizeof(response_t));
      dummy->status = STATUS_ERROR;
      responses[i] = dummy;
      continue;
    }

    response->host = strdup(host);
    response->status = STATUS_PENDING;
    response->req_time_start = time(NULL);
    responses[i] = response;

    // Resolve hostname
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
      response->status = STATUS_ERROR;
      ERROR_PRINT("DNS resolution failed for host: %s", host);
      continue;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
      response->status = STATUS_ERROR;
      ERROR_PRINT("Socket creation failed for host: %s", host);
      freeaddrinfo(res);
      continue;
    }

    // Force socket to close immediately (no lingering in TIME_WAIT)
    struct linger so_linger = {1, 0};  // on, 0 seconds
    setsockopt(sock, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));

    // Non-blocking connect
    fcntl(sock, F_SETFL, O_NONBLOCK);
    connect(sock, res->ai_addr, res->ai_addrlen);

    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);
    struct timeval tv = {CONNECT_TIMEOUT_SEC, 0};

    int sel = select(sock + 1, NULL, &writefds, NULL, &tv);
    if (sel <= 0) {
      response->status = STATUS_TIMEOUT;
      WARNING_PRINT("Connection timeout to host: %s", host);
      close(sock);
      freeaddrinfo(res);
      continue;
    }

    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
      response->status = STATUS_ERROR;
      ERROR_PRINT("Connect failed to host: %s (errno: %d)", host, err);
      close(sock);
      freeaddrinfo(res);
      continue;
    }

    // Restore blocking mode before sending
    // fcntl(sock, F_SETFL, 0);
    int old_flags = fcntl(sock, F_GETFL, 0);
    if (old_flags == -1) {
      ERROR_PRINT("fcntl(F_GETFL) failed on socket");
    } else if (fcntl(sock, F_SETFL, old_flags & ~O_NONBLOCK) == -1) {
      ERROR_PRINT("fcntl(F_SETFL) failed while restoring blocking mode");
    }

    unsigned char* compressed_message = NULL;
    size_t compressed_length = 0;
    size_t msg_len = strlen(message) + 1;
    if (!compress_gzip_with_prefix((const unsigned char *)message, msg_len, &compressed_message, &compressed_length)) {
      response->status = STATUS_ERROR;
      ERROR_PRINT("Failed to compress message with gzip and add prefix");
      close(sock);
      freeaddrinfo(res);
      continue;
    }

    ssize_t sent = send(sock, compressed_message, compressed_length, 0);
    free(compressed_message);
    if (sent < 0) {
      response->status = STATUS_ERROR;
      ERROR_PRINT("Send failed to host: %s", host);
    } else {
      response->status = STATUS_OK;
    }

    response->req_time_end = time(NULL);
    DEBUG_PRINT("Host: %s | Status: %s | Time: %lds",
           host,
           response->status == STATUS_OK ? "OK" :
           response->status == STATUS_TIMEOUT ? "TIMEOUT" : "ERROR",
           response->req_time_end - response->req_time_start);
    close(sock);
    freeaddrinfo(res);
  }

  return responses;
}

void cleanup_responses(response_t** responses) {
  int i = 0;
  while (responses && responses[i] != NULL) {
    free(responses[i]->host);
    free(responses[i]->data); // Will be NULL, still safe to free
    free(responses[i]);
    i++;
  }
  free(responses);
}