#include "uv_net_multi.h"

void on_close(uv_handle_t* handle) {
    client_t* client = (client_t*)handle->data;
    if (client) {
        client->is_closing = 1;
        client->response->req_time_end = time(NULL);

        uv_timer_stop(&client->timer);

        if (client->response) {
            free(client->response->host);
            free(client->response->data);
            free(client->response);
        }

        free(client);
    }
}

void safe_close(client_t* client) {
    if (!client || uv_is_closing((uv_handle_t*)&client->handle)) {
        return;
    }

    uv_close((uv_handle_t*)&client->timer, NULL);
    uv_close((uv_handle_t*)&client->handle, on_close);
}

void on_timeout(uv_timer_t* timer) {
    client_t* client = (client_t*)timer->data;
    if (client) {
        client->response->status = STATUS_TIMEOUT;
        safe_close(client);
    }
}

void alloc_buffer_multi(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    (void)suggested_size;
    (void)handle;

    buf->base = (char*)malloc(TRANSFER_BUFFER_SIZE);
    if (!buf->base) {
        ERROR_PRINT("Memory allocation failed in alloc_buffer_multi()");
        buf->len = 0;
        buf->base = NULL;
        return;
    }

    buf->len = TRANSFER_BUFFER_SIZE;
}

void on_write(uv_write_t* req, int status) {
    client_t* client = (client_t*)req->data;
    char* msg_copy = (char*)req->data;

    if (status < 0) {
        ERROR_PRINT("Write error: %s", uv_strerror(status));
        safe_close(client);
    }

    if (msg_copy) {
        free(msg_copy);
    }

    uv_timer_stop(&client->timer);
    uv_timer_start(&client->timer, on_timeout, RESPONSE_TIMEOUT, 0);
    uv_read_start((uv_stream_t*)req->handle, alloc_buffer_multi, on_read);
}

void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    client_t* client = (client_t*)stream->data;

    if (nread > 0) {
        char* new_data = realloc(client->response->data, client->response->size + nread);
        if (!new_data) {
            ERROR_PRINT("Memory allocation failed in on_read()");
            safe_close(client);
            return;
        }
        client->response->data = new_data;
        memcpy(client->response->data + client->response->size, buf->base, nread);
        client->response->size += nread;

        uv_timer_stop(&client->timer);
        uv_timer_start(&client->timer, on_timeout, RESPONSE_TIMEOUT, 0);
    } else if (nread < 0) {
        client->response->status = (nread == UV_EOF) ? STATUS_OK : STATUS_ERROR;
        uv_timer_stop(&client->timer);
        safe_close(client);
    }

    if (buf->base) {
        free(buf->base);
    }
}

void retry_connection(uv_timer_t* timer) {
    client_t* client = (client_t*)timer->data;

    if (client->retry_count >= MAX_RETRIES) {
        ERROR_PRINT("Max retries reached for %s, marking as failed.", client->response->host);
        client->response->status = STATUS_ERROR;
        safe_close(client);
        return;
    }

    INFO_PRINT("Retrying connection to %s (%d/%d)...", client->response->host, client->retry_count + 1, MAX_RETRIES);

    safe_close(client);
    client->retry_count++;

    uv_tcp_init(uv_default_loop(), &client->handle);
    client->handle.data = client;

    struct sockaddr_in dest;
    uv_ip4_addr(client->response->host, 18281, &dest);
    start_connection(client, (const struct sockaddr*)&dest);
}

void on_connect(uv_connect_t* req, int status) {
    client_t* client = (client_t*)req->data;

    if (!client || client->is_closing) {
        return;
    }

    if (status < 0) {
        ERROR_PRINT("Connection failed: %s (Attempt %d/%d)\n", uv_strerror(status), client->retry_count, MAX_RETRIES);

        if (client->retry_count < MAX_RETRIES) {
            uv_timer_init(uv_default_loop(), &client->timer);
            client->timer.data = client;
            uv_timer_start(&client->timer, retry_connection, RETRY_DELAY_MS, 0);
            return;
        }

        client->response->status = STATUS_ERROR;
        safe_close(client);
        return;
    }

    client->retry_count = 0;
    uv_timer_stop(&client->timer);
    uv_timer_start(&client->timer, on_timeout, RESPONSE_TIMEOUT, 0);

    char* msg_copy = strdup(client->message);
    if (!msg_copy) {
        ERROR_PRINT("Memory allocation failed for message copy");
        safe_close(client);
        return;
    }

    uv_buf_t buf = uv_buf_init(msg_copy, strlen(msg_copy));
    client->write_req.data = msg_copy;
    uv_write(&client->write_req, (uv_stream_t*)&client->handle, &buf, 1, on_write);
}

void start_connection(client_t* client, const struct sockaddr* addr) {
    if (!client) return;

    DEBUG_PRINT("Starting connection to %s\n", client->response->host);
    uv_tcp_connect(&client->connect_req, &client->handle, addr, on_connect);
    DEBUG_PRINT("NEXT........")




}

void on_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res) {
    client_t* client = resolver->data;

    if (status == 0 && res != NULL) {
        DEBUG_PRINT("Resolved hostname: %s -> %s\n", client->response->host, inet_ntoa(((struct sockaddr_in*)res->ai_addr)->sin_addr));
        start_connection(client, res->ai_addr);
    } else {
        ERROR_PRINT("DNS resolution failed for %s: %s\n", client->response->host, uv_strerror(status));

        if (client->response) {
            free(client->response->host);
            free(client->response->data);
            free(client->response);
        }

        safe_close(client);
    }

    uv_freeaddrinfo(res);
    free(resolver);
}

response_t** send_multi_request(const char **hosts, int port, const char* message) {
    if (!hosts || !message) return NULL;

    int total_hosts = 0;
    while (hosts[total_hosts] != NULL) total_hosts++;
    if (total_hosts == 0) return NULL;

    char port_str[6];
    sprintf(port_str, "%d", port);

    uv_loop_t* loop = uv_default_loop();
    response_t** responses = calloc(total_hosts + 1, sizeof(response_t*));

    for (int i = 0; i < total_hosts; i++) {
        if (!hosts[i]) continue;

        client_t* client = calloc(1, sizeof(client_t));
        client->message = strdup(message);
        client->response = calloc(1, sizeof(response_t));
        client->response->host = strdup(hosts[i]);
        responses[i] = client->response;

        uv_tcp_init(loop, &client->handle);
        client->handle.data = client;
        uv_timer_init(loop, &client->timer);
        client->timer.data = client;

        struct sockaddr_in dest;
        uv_ip4_addr(hosts[i], port, &dest);
        start_connection(client, (const struct sockaddr*)&dest);
    }

    uv_run(loop, UV_RUN_DEFAULT);

    for (int i = 0; i < total_hosts; i++) {
        if (responses[i]) {
            free(responses[i]->host);
            free(responses[i]->data);
            free(responses[i]);
        }
    }
    free(responses);

    return responses;
}

void cleanup_responses(response_t** responses) {
    if (!responses) return;  // Check if responses is NULL

    for (int i = 0; responses[i] != NULL; i++) {
        free(responses[i]->host);
        free(responses[i]->data);
        free(responses[i]->client);
        free(responses[i]);  // Free the response struct itself
        responses[i] = NULL;
    }

    free(responses);  // Free the array of pointers
    responses = NULL;
}