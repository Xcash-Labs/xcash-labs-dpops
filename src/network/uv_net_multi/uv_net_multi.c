#include "uv_net_multi.h"

void on_close(uv_handle_t* handle) {
  client_t* client = (client_t*)handle->data;
  DEBUG_PRINT("on_close() triggered for %s", client->response->host);
  client->response->req_time_end = time(NULL);
}

void safe_close(client_t* client) {
  if (client->is_closing) return;
  client->is_closing = 1;

  if (!uv_is_closing((uv_handle_t*)&client->timer)) {
    uv_close((uv_handle_t*)&client->timer, NULL);
  }
  if (!uv_is_closing((uv_handle_t*)&client->handle)) {
    uv_close((uv_handle_t*)&client->handle, on_close);
  }
}

void on_timeout(uv_timer_t* timer) {
  client_t* client = (client_t*)timer->data;

  // If response already completed, skip timeout
  if (client->response->status == STATUS_OK || client->is_closing) {
    DEBUG_PRINT("Timeout triggered, but client %s is already closing or response OK", 
                client->response ? client->response->host : "unknown");
    return;
  }

  ERROR_PRINT("Write operation timed out for %s", 
              client->response ? client->response->host : "unknown");

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
    ERROR_PRINT("Write error to %s: %s",
                client->response ? client->response->host : "unknown",
                uv_strerror(status));
    client->response->status = STATUS_ERROR;
    safe_close(client);
    return;
  }
  uv_timer_stop(&client->timer);
  safe_close(client);
}

void on_connect_nodbug(uv_connect_t* req, int status) {
  client_t* client = (client_t*)req->data;
  if (client->is_closing || status < 0) {
    // Handle connection error
    DEBUG_PRINT("Connection error %s: %s", client->response->host, uv_strerror(status));
    client->response->status = STATUS_ERROR;
    safe_close(client);
    return;
  }
  // stop connection timeout timer
  uv_timer_stop(&client->timer);
  // Start the timer to wait for write operation
  uv_timer_start(&client->timer, on_timeout, UV_WRITE_TIMEOUT, 0);
  // Write the message to the server
  uv_buf_t buf = uv_buf_init((char*)(uintptr_t)client->message, strlen(client->message));
  uv_write(&client->write_req, (uv_stream_t*)&client->handle, &buf, 1, on_write);
}

void on_connect(uv_connect_t* req, int status) {
  client_t* client = (client_t*)req->data;

  if (client->is_closing || status < 0) {
    DEBUG_PRINT("Connection error to %s: %s",
                client->response ? client->response->host : "unknown",
                uv_strerror(status));
    client->response->status = STATUS_ERROR;
    safe_close(client);
    return;
  }

  DEBUG_PRINT("Successfully connected to %s", client->response->host);

  // Stop connection timeout timer
  uv_timer_stop(&client->timer);
  DEBUG_PRINT("Connection timeout timer stopped for %s", client->response->host);

  // Start the timer to wait for write operation
  uv_timer_start(&client->timer, on_timeout, UV_WRITE_TIMEOUT, 0);
  DEBUG_PRINT("Write timeout timer started for %s (timeout = %d ms)", client->response->host, UV_WRITE_TIMEOUT);

  // Write the message to the server
  uv_buf_t buf = uv_buf_init((char*)(uintptr_t)client->message, strlen(client->message));
  int rc = uv_write(&client->write_req, (uv_stream_t*)&client->handle, &buf, 1, on_write);
  if (rc < 0) {
    ERROR_PRINT("uv_write() failed for %s: %s", client->response->host, uv_strerror(rc));
    client->response->status = STATUS_ERROR;
    safe_close(client);
    return;
  }

  DEBUG_PRINT("uv_write() initiated to %s", client->response->host);
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

response_t** send_multi_request(const char** hosts, int port, const char* message) {
  // count the number of hosts
  int total_hosts = 0;
  while (hosts[total_hosts] != NULL) total_hosts++;
  if (total_hosts == 0)
    return NULL;

  char port_str[6];  // maximum 0..65535 + \0
  sprintf(port_str, "%d", port);

  uv_loop_t* loop = uv_default_loop();

  response_t** responses = calloc(total_hosts + 1, sizeof(response_t*));
  if (!responses) {
    ERROR_PRINT("[ERROR] Failed to allocate memory for responses.");
    return NULL;
  }

  for (int i = 0; i < total_hosts; i++) {
    // Initialize each client structure
    client_t* client = calloc(1, sizeof(client_t));
    client->message = message;
    client->response = (response_t*)calloc(1, sizeof(response_t));
    client->response->host = strdup(hosts[i]);
    client->response->status = STATUS_PENDING;
    client->response->client = client;
    client->response->req_time_start = time(NULL);
    responses[i] = client->response;
    uv_timer_init(loop, &client->timer);
    client->timer.data = client;

    // Start the connection timeout timer
    uv_timer_start(&client->timer, on_timeout, UV_CONNECTION_TIMEOUT, 0);

    uv_tcp_init(loop, &client->handle);
    client->handle.data = client;
    client->connect_req.data = client;
    client->write_req.data = client;

    if (is_ip_address(hosts[i])) {
      struct sockaddr_in dest;
      uv_ip4_addr(hosts[i], port, &dest);
      start_connection(client, (const struct sockaddr*)&dest);
    } else {
      uv_getaddrinfo_t* resolver = malloc(sizeof(uv_getaddrinfo_t));
      struct addrinfo hints;
      memset(&hints, 0, sizeof(hints));
      hints.ai_family = PF_INET;
      hints.ai_socktype = SOCK_STREAM;
      resolver->data = client;
      uv_getaddrinfo(uv_default_loop(), resolver, on_resolved, hosts[i], port_str, &hints);
    }
  }

  uv_run(loop, UV_RUN_DEFAULT);

  int result = uv_loop_close(loop);
  if (result != 0) {
    DEBUG_PRINT("Error closing loop: %s\n", uv_strerror(result));
  }

  return responses;
}

void cleanup_responses(response_t** responses) {
  int i = 0;
  while (responses && responses[i] != NULL) {
    free(responses[i]->host);
    free(responses[i]->data);
    free(responses[i]->client);
    free(responses[i]);
    responses[i] = NULL;
    i++;
  };
  free(responses);
}