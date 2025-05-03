#include "uv_net_multi.h"

const char* status_to_string(int status) {
  switch (status) {
    case STATUS_OK: return "OK";
    case STATUS_ERROR: return "ERROR";
    case STATUS_TIMEOUT: return "TIMEOUT";
    case STATUS_PENDING: return "PENDING";
    default: return "UNKNOWN";
  }
}

void on_close(uv_handle_t* handle) {
  client_t* client = (client_t*)handle->data;
  DEBUG_PRINT("on_close() triggered for %s", client->response->host);
  client->response->req_time_end = time(NULL);
}

void safe_close(client_t* client) {
  if (!client) return;

  if (client->is_closing) {
    DEBUG_PRINT("safe_close: already closing %s", client->response ? client->response->host : "unknown");
    return;
  }

  client->is_closing = 1;

  // Stop timer explicitly if still active
  if (uv_is_active((uv_handle_t*)&client->timer)) {
    uv_timer_stop(&client->timer);
  }

  // Then close it
  if (!uv_is_closing((uv_handle_t*)&client->timer)) {
    uv_close((uv_handle_t*)&client->timer, NULL);
  }

  // Close client stream
  if (!uv_is_closing((uv_handle_t*)&client->handle)) {
    uv_close((uv_handle_t*)&client->handle, on_close);
  }
}

void on_timeout(uv_timer_t* timer) {
  client_t* client = (client_t*)timer->data;

  if (client->is_closing || client->response->status == STATUS_OK) {
    return;
  }

  const char* phase = client->write_complete ? "read" : "write";
  ERROR_PRINT("Timeout during %s phase from %s", phase, client->response->host);

  client->response->status = STATUS_TIMEOUT;
  safe_close(client);
}

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  (void)handle;
  suggested_size = TRANSFER_BUFFER_SIZE;
  buf->base = (char*)malloc(suggested_size);
  buf->len = suggested_size;
}

void on_write(uv_write_t* req, int status) {
  client_t* client = (client_t*)req->data;
 
  if (status < 0) {
    ERROR_PRINT("Write error: %s", uv_strerror(status));
    client->response->status = STATUS_ERROR;
    safe_close(client);
    return;
  }

  uv_timer_stop(&client->timer);
  client->write_complete = 1;

    // If you are NOT reading a response, close the connection
    client->response->status = STATUS_OK;  // Mark as successful
    safe_close(client);
}

void on_connect(uv_connect_t* req, int status) {
  client_t* client = (client_t*)req->data;

  if (client->is_closing || status < 0) {
    DEBUG_PRINT("Skipping connection to %s: status=%d, is_closing=%d", 
                client->response->host, status, client->is_closing);
    DEBUG_PRINT("Connection error %s: %s", client->response->host, uv_strerror(status));
    client->response->status = STATUS_ERROR;
    safe_close(client);
    return;
  }

  DEBUG_PRINT("Successfully connected to %s", client->response->host);

  // Stop the connection timeout
  uv_timer_stop(&client->timer);
  DEBUG_PRINT("Connect timeout timer stopped for %s", client->response->host);

  // Prepare write buffer
  uv_buf_t buf = uv_buf_init((char *)(uintptr_t)client->message, strlen(client->message));
  client->write_req.data = client;

  DEBUG_PRINT("Preparing to send message to %s, length: %zu", client->response->host, strlen(client->message));

  // Start write timeout timer
  int timer_rc = uv_timer_start(&client->timer, on_timeout, UV_WRITE_TIMEOUT, 0);
  if (timer_rc < 0) {
    ERROR_PRINT("Failed to start write timeout for %s: %s", client->response->host, uv_strerror(timer_rc));
    client->response->status = STATUS_ERROR;
    safe_close(client);
    return;
  }
  DEBUG_PRINT("Write timeout timer started for %s (timeout = %d ms)", client->response->host, UV_WRITE_TIMEOUT);

  // Start the actual write
  client->write_complete = 0;
  int rc = uv_write(&client->write_req, (uv_stream_t*)&client->handle, &buf, 1, on_write);
  if (rc < 0) {
    ERROR_PRINT("uv_write() failed for %s: %s", client->response->host, uv_strerror(rc));
    client->response->status = STATUS_ERROR;
    safe_close(client);
    return;
  }

  DEBUG_PRINT("uv_write() started to %s", client->response->host);
}

int is_ip_address(const char* host) {
  struct in_addr sa;
  return inet_pton(AF_INET, host, &(sa.s_addr));
}

void start_connection(client_t* client, const struct sockaddr* addr) {
  uv_tcp_connect(&client->connect_req, &client->handle, addr, on_connect);
}

void on_resolved(uv_getaddrinfo_t* resolver, int status, struct addrinfo* res) {
  client_t* client = resolver->data;

  if (status == 0 && res != NULL) {
    if (!client->is_closing) {
      start_connection(client, res->ai_addr);
    }
  } else {
    DEBUG_PRINT("DNS resolution failed for %s: %s", client->response->host, uv_strerror(status));
    client->response->status = STATUS_ERROR;
  }

  if (res) {
    uv_freeaddrinfo(res);
  }

  free(resolver);
}










bool send_multi_request(const char** hosts, int port, const char* message) {
  int total_hosts = 0;
  while (hosts[total_hosts] != NULL) total_hosts++;
  if (total_hosts == 0) return NULL;

  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%d", port);

  uv_loop_t* loop = uv_default_loop();

  for (int i = 0; i < total_hosts; i++) {
    client_t* client = calloc(1, sizeof(client_t));
    if (!client) {
      ERROR_PRINT("Failed to allocate memory for client.");
      continue;
    }

    client->message = message;

    // Initialize libuv handles
    if (uv_timer_init(loop, &client->timer) < 0 ||
        uv_tcp_init(loop, &client->handle) < 0) {
      ERROR_PRINT("libuv handle init failed for host %s", hosts[i]);
      client->response->status = STATUS_ERROR;
      continue;
    }

    client->timer.data = client;
    client->handle.data = client;
    client->connect_req.data = client;
    client->write_req.data = client;

    // Start timeout for connection phase
    uv_timer_start(&client->timer, on_timeout, UV_CONNECTION_TIMEOUT, 0);

    if (is_ip_address(hosts[i])) {
      struct sockaddr_in dest;
      if (uv_ip4_addr(hosts[i], port, &dest) == 0) {
        start_connection(client, (const struct sockaddr*)&dest);
      } else {
        ERROR_PRINT("Failed to resolve IP for %s", hosts[i]);
        client->response->status = STATUS_ERROR;
      }
    } else {
      uv_getaddrinfo_t* resolver = malloc(sizeof(uv_getaddrinfo_t));
      if (!resolver) {
        ERROR_PRINT("Failed to allocate resolver for %s", hosts[i]);
        client->response->status = STATUS_ERROR;
        continue;
      }

      struct addrinfo hints = {0};
      hints.ai_family = PF_INET;
      hints.ai_socktype = SOCK_STREAM;
      resolver->data = client;

      int rc = uv_getaddrinfo(loop, resolver, on_resolved, hosts[i], port_str, &hints);
      if (rc != 0) {
        ERROR_PRINT("uv_getaddrinfo failed for %s: %s", hosts[i], uv_strerror(rc));
        free(resolver);
        client->response->status = STATUS_ERROR;
      }
    }
  }

  uv_run(loop, UV_RUN_DEFAULT);

  int result = uv_loop_close(loop);
  if (result != 0) {
    DEBUG_PRINT("Error closing loop: %s", uv_strerror(result));
  }

  return XCASH_ok;
}