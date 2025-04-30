#include "uv_net_server.h"

static uv_loop_t loop;
static uv_tcp_t server;
static pthread_t uv_thread;
static uv_async_t async_shutdown;  // Async handle for clean shutdown

void check_if_ready_to_close(server_client_t *client);

void handle_message_work(uv_work_t *req) {
    message_work_t *work = (message_work_t *)req->data;
    handle_srv_message(work->data, work->data_len, work->client);
}

void handle_message_after(uv_work_t *req, int status) {
    message_work_t *work = (message_work_t *)req->data;
    DEBUG_PRINT("Finished background message processing from %s", work->client->client_ip);

    work->client->received_reply = true;
    check_if_ready_to_close(work->client);

    free(work->data);
    free(work);
    free(req);
}

// Cleanup function after client disconnects
void on_client_close(uv_handle_t *handle) {
  if (handle) {
    free(handle);  // Free memory allocated for the client
  }
}

void check_if_ready_to_close(server_client_t *client) {
  if (client->sent_reply && client->received_reply) {
    INFO_PRINT("Round-trip with %s complete. Closing connection.", client->client_ip);
    uv_read_stop((uv_stream_t *)&client->handle);
    uv_close((uv_handle_t *)&client->handle, on_client_close);
  }
}

void get_client_ip(server_client_t *client) {
  struct sockaddr_storage addr;
  int namelen = sizeof(addr);

  int status = uv_tcp_getpeername((uv_tcp_t *)&client->handle, (struct sockaddr *)&addr, &namelen);
  if (status != 0) {
    strncpy(client->client_ip, "Unknown", sizeof(client->client_ip));
    client->client_ip[sizeof(client->client_ip) - 1] = '\0';  // Ensure null termination
    ERROR_PRINT("Error retrieving client IP: %s", uv_strerror(status));
    return;
  }

  if (addr.ss_family == AF_INET) {
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    uv_inet_ntop(AF_INET, &s->sin_addr, client->client_ip, sizeof(client->client_ip));
  } else if (addr.ss_family == AF_INET6) {
    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
    uv_inet_ntop(AF_INET6, &s->sin6_addr, client->client_ip, sizeof(client->client_ip));
  }
}

void on_new_connection(uv_stream_t *server_handle, int status) {
  if (status < 0) {
    ERROR_PRINT("Error on new connection: %s", uv_strerror(status));
    return;
  }

  server_client_t *client = (server_client_t *)malloc(sizeof(server_client_t));
  if (!client) {
    ERROR_PRINT("Memory allocation failed for client");
    return;
  }

  memset(client, 0, sizeof(server_client_t));

  if (uv_tcp_init(server_handle->loop, (uv_tcp_t *)&client->handle) < 0) {
    ERROR_PRINT("Failed to initialize TCP handle for client");
    free(client);
    return;
  }

  if (uv_accept(server_handle, (uv_stream_t *)&client->handle) == 0) {
    get_client_ip(client);  // Store client IP
    DEBUG_PRINT("New connection from: %s", client->client_ip);

    if (uv_read_start((uv_stream_t *)&client->handle, alloc_buffer_srv, on_client_read) < 0) {
      ERROR_PRINT("Failed to start reading from client");
      uv_close((uv_handle_t *)&client->handle, on_client_close);
      return;
    }
  } else {
    uv_close((uv_handle_t *)&client->handle, on_client_close);
  }
}

// Allocate buffer for reading
void alloc_buffer_srv(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  (void)handle;  // Unused parameter

  // Ensure a minimum allocation size
  size_t buffer_size = (suggested_size > SMALL_BUFFER_SIZE) ? suggested_size : SMALL_BUFFER_SIZE;

  // Allocate memory safely
  buf->base = (char *)calloc(1, buffer_size);
  if (!buf->base) {
    ERROR_PRINT("Memory allocation failed in alloc_buffer_srv()");
    buf->len = 0;
    return;
  }

  buf->len = buffer_size;
}

// Read data from client
void on_client_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
  server_client_t *client_data = (server_client_t *)client;

  if (nread > 0) {
    DEBUG_PRINT("Received data: %.*s", (int)nread, buf->base);

    message_work_t *work_data = malloc(sizeof(message_work_t));
    if (!work_data) {
      ERROR_PRINT("Failed to allocate work data");
      free(buf->base);
      return;
    }

    work_data->client = client_data;
    work_data->data = strndup(buf->base, nread);
    if (!work_data->data) {
      free(work_data);
      ERROR_PRINT("Failed to allocate data copy");
      free(buf->base);
      return;
    }

    work_data->data_len = nread;

    uv_work_t *req = malloc(sizeof(uv_work_t));
    if (!req) {
      free(work_data->data);
      free(work_data);
      ERROR_PRINT("Failed to allocate uv_work_t");
      free(buf->base);
      return;
    }

    req->data = work_data;

    uv_queue_work(
        uv_default_loop(),
        req,
        handle_message_work,
        handle_message_after
    );

  } else if (nread == UV_EOF) {
    DEBUG_PRINT("Client disconnected.");
    uv_read_stop(client);
    uv_close((uv_handle_t *)client, on_client_close);
  } else if (nread < 0) {
    ERROR_PRINT("Read error: %s", uv_strerror(nread));
    uv_read_stop(client);
    uv_close((uv_handle_t *)client, on_client_close);
  }

  if (buf && buf->base) {
    free(buf->base);
  }
}

// Thread-safe shutdown callback
void on_shutdown_signal(uv_async_t *async) {
  DEBUG_PRINT("Shutting down UV event loop...");
  uv_close((uv_handle_t *)&server, NULL);
  uv_close((uv_handle_t *)async, NULL);
  uv_stop(&loop);
}

// Function to run the libuv loop in a separate thread
void *uv_run_thread(void *arg) {
  (void)arg;
  uv_run(&loop, UV_RUN_DEFAULT);
  uv_loop_close(&loop);
  return NULL;
}

// Start TCP server (Runs in a separate thread)
bool start_tcp_server(int port) {
  struct sockaddr_in addr;

  // Initialize the loop
  uv_loop_init(&loop);
  uv_tcp_init(&loop, &server);
  uv_async_init(&loop, &async_shutdown, on_shutdown_signal);

  // Bind to given port
  uv_ip4_addr("0.0.0.0", port, &addr);
  if (uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0) < 0) {
    ERROR_PRINT("Failed to bind to port %d", port);
    uv_loop_close(&loop);
    return XCASH_ERROR;
  }

  // Start listening for connections
  if (uv_listen((uv_stream_t *)&server, MAX_CONNECTIONS, on_new_connection) < 0) {
    ERROR_PRINT("Failed to listen on port %d", port);
    uv_loop_close(&loop);
    return XCASH_ERROR;
  }

  INFO_PRINT("Server listening on port %d", port);

  // Run in a new thread
  if (pthread_create(&uv_thread, NULL, uv_run_thread, NULL) != 0) {
    ERROR_PRINT("Failed to create UV event loop thread.");
    uv_loop_close(&loop);
    return XCASH_ERROR;
  }

  return XCASH_OK;
}

// Helper function to close all handles
void close_callback(uv_handle_t *handle, void *arg) {
  (void)arg;
  if (!uv_is_closing(handle)) {
    uv_close(handle, NULL);
  }
}

void stop_tcp_server() {
  INFO_PRINT("Stopping TCP server...");
  // Walk through all handles and close them
  uv_walk(&loop, close_callback, NULL);
  // Wait for handles to close
  int attempts = 7;  // Max attempts to wait for cleanup
  while (uv_loop_alive(&loop) && attempts-- > 0) {
    INFO_PRINT("Waiting for handles to close...");
    uv_run(&loop, UV_RUN_NOWAIT);
    usleep(500000);  // Sleep 500ms to give time for handles to close
  }
  uv_stop(&loop);
  if (uv_loop_close(&loop) != 0) {
    ERROR_PRINT("Failed to close the event loop. Some handles are still open.");
  } else {
    INFO_PRINT("Event loop closed successfully.");
  }
}

void on_write_complete(uv_write_t *req, int status);
void on_write_timeout(uv_timer_t *timer);
void on_generic_close(uv_handle_t *handle);  // New helper

void send_data_uv(server_client_t *client, const char *message) {
  if (!client || !message) {
    ERROR_PRINT("Invalid parameters in send_data_uv");
    return;
  }

  size_t length = strlen(message);
  if (length >= MAXIMUM_BUFFER_SIZE) {
    length = MAXIMUM_BUFFER_SIZE - 1;  // Truncate safely
  }

  write_srv_request_t *write_req = malloc(sizeof(write_srv_request_t));
  if (!write_req) {
    ERROR_PRINT("Memory allocation failed for write_srv_request_t");
    return;
  }

  write_req->message_copy = strndup(message, length);
  if (!write_req->message_copy) {
    ERROR_PRINT("Memory allocation failed for message copy");
    free(write_req);
    return;
  }

  write_req->client = client;

  uv_buf_t buf = uv_buf_init(write_req->message_copy, length);

  int result = uv_write(&write_req->req, (uv_stream_t *)&client->handle, &buf, 1, on_write_complete);
  if (result < 0) {
    ERROR_PRINT("uv_write error: %s", uv_strerror(result));
    free(write_req->message_copy);
    free(write_req);
    return;
  }

  // Initialize the timer for timeout
  uv_timer_init(uv_default_loop(), &write_req->timer);
  write_req->timer.data = write_req;
  uv_timer_start(&write_req->timer, on_write_timeout, UV_SEND_TIMEOUT, 0);
}

void on_write_complete(uv_write_t *req, int status) {
  write_srv_request_t *write_req = (write_srv_request_t *)req;
  server_client_t *client = write_req->client;

  if (status < 0) {
    ERROR_PRINT("Write error: %s", uv_strerror(status));
  } else {
    DEBUG_PRINT("Message sent successfully");
    client->sent_reply = true;
  }

  uv_timer_stop(&write_req->timer);
  uv_close((uv_handle_t *)&write_req->timer, on_generic_close);

  // 💡 Check if both flags are true
  check_if_ready_to_close(client);
}

void on_write_timeout(uv_timer_t *timer) {
  write_srv_request_t *write_req = (write_srv_request_t *)timer->data;

  ERROR_PRINT("Write operation timed out");

  uv_close((uv_handle_t *)&write_req->timer, NULL);  // Safe, already will be handled

  // Close the client connection (force disconnect)
  uv_close((uv_handle_t *)&write_req->client->handle, on_generic_close);  // Defer free after client close
}

void on_generic_close(uv_handle_t *handle) {
  write_srv_request_t *write_req = (write_srv_request_t *)handle->data;

  if (write_req) {
    if (write_req->message_copy) {
      free(write_req->message_copy);
      write_req->message_copy = NULL;
    }
    free(write_req);
  }
}