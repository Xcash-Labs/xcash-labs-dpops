#include "uv_net_server.h"

static uv_loop_t loop;
static uv_tcp_t server;


// ✅ Handle client connections
void on_new_connection(uv_stream_t *server_handle, int status) {
    if (status < 0) {
        ERROR_PRINT("Error on new connection: %s", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t *) malloc(sizeof(uv_tcp_t));
    if (!client) {
        ERROR_PRINT("Memory allocation failed for client");
        return;
    }

    uv_tcp_init(&loop, client);

    if (uv_accept(server_handle, (uv_stream_t *) client) == 0) {
        DEBUG_PRINT("New connection accepted.");
        uv_read_start((uv_stream_t *) client, alloc_buffer, on_client_read);
    } else {
        uv_close((uv_handle_t *) client, NULL);
        free(client);
    }
}

// ✅ Allocate buffer for reading
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void)handle;  // Suppress unused parameter warning

    buf->base = (char *) malloc(suggested_size);
    if (!buf->base) {
        ERROR_PRINT("Memory allocation failed in alloc_buffer()");
        buf->len = 0;
        return;
    }
    buf->len = suggested_size;
}

// ✅ Read data from client
void on_client_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        DEBUG_PRINT("Received data: %.*s", (int)nread, buf->base);
    } else if (nread < 0) {
        if (nread == UV_EOF) {
            DEBUG_PRINT("Client disconnected.");
        } else {
            ERROR_PRINT("Read error: %s", uv_strerror(nread));
        }
        uv_close((uv_handle_t *) client, NULL);
        free(client);
    }
    free(buf->base);
}

// ✅ Start TCP server (Callable from xcash_dpops)
int start_tcp_server(int port) {
    struct sockaddr_in addr;

    // ✅ Initialize the loop
    uv_loop_init(&loop);
    uv_tcp_init(&loop, &server);

    // ✅ Bind to given port
    uv_ip4_addr("0.0.0.0", port, &addr);
    if (uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0) < 0) {
        ERROR_PRINT("Failed to bind to port %d", port);
        return XCASH_ERROR;
    }

    // ✅ Start listening for connections
    if (uv_listen((uv_stream_t *)&server, MAX_CONNECTIONS, on_new_connection) < 0) {
        ERROR_PRINT("Failed to listen on port %d", port);
        return XCASH_ERROR;
    }

    DEBUG_PRINT("Server listening on port %d", port);

    // ✅ Run the event loop
    uv_run(&loop, UV_RUN_DEFAULT);

    return XCASH_OK;
}