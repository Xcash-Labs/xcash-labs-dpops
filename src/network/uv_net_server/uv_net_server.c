#include "uv_net_server.h"

static uv_loop_t loop;
static uv_tcp_t server;
static pthread_t uv_thread;
static uv_async_t async_shutdown; // Async handle for clean shutdown

// Cleanup function after client disconnects
void on_client_close(uv_handle_t *handle) {
    if (handle) {
        free(handle);  // Free memory allocated for the client
    }
}

// Handle client connections
void on_new_connection(uv_stream_t *server_handle, int status) {
    if (status < 0) {
        ERROR_PRINT("Error on new connection: %s", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    if (!client) {
        ERROR_PRINT("Memory allocation failed for client");
        return;
    }
    
    // Initialize the TCP handle for the client
    if (uv_tcp_init(server_handle->loop, client) < 0) {  // Use server's loop
        ERROR_PRINT("Failed to initialize TCP handle for client");
        free(client);  // Free memory if initialization fails
        return;
    }

    if (uv_accept(server_handle, (uv_stream_t *)client) == 0) {
        DEBUG_PRINT("New connection accepted.");

        // Start reading from the client
        if (uv_read_start((uv_stream_t *)client, alloc_buffer_srv, on_client_read) < 0) {
            ERROR_PRINT("Failed to start reading from client");
            uv_close((uv_handle_t *)client, on_client_close);
            return;
        }
    } else {
        uv_close((uv_handle_t *)client, on_client_close);  // Close if accept fails
    }
}

// Allocate buffer for reading
void alloc_buffer_srv(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void)handle;

    buf->base = (char *) malloc(suggested_size);
    if (!buf->base) {
        ERROR_PRINT("Memory allocation failed in alloc_buffer_srv()");
        buf->len = 0;
        return;
    }
    buf->len = suggested_size;
}

// Read data from client
void on_client_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        DEBUG_PRINT("Received data: %.*s", (int)nread, buf->base);
    } else if (nread == UV_EOF) {
        ERROR_PRINT("Client disconnected.");
        uv_read_stop(client);  // Stop reading if the client disconnected
        uv_close((uv_handle_t *)client, on_client_close);
    } else if (nread < 0) {
        ERROR_PRINT("Read error: %s", uv_strerror(nread));
        uv_read_stop(client);  // Stop reading if there was an error
        uv_close((uv_handle_t *)client, on_client_close);
    }

    // Free buf->base only if it is not NULL
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