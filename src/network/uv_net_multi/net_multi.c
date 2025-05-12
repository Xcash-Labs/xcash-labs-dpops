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
    if (!response) continue;

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

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
      response->status = STATUS_ERROR;
      continue;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
      response->status = STATUS_ERROR;
      freeaddrinfo(res);
      continue;
    }

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
      close(sock);
      freeaddrinfo(res);
      continue;
    }

    // Restore blocking mode before sending
    fcntl(sock, F_SETFL, 0);

    ssize_t sent = send(sock, message, strlen(message), 0);
    if (sent < 0) {
      response->status = STATUS_ERROR;
    } else {
      response->status = STATUS_OK;
    }

    response->req_time_end = time(NULL);
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
