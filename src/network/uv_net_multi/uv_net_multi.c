#include "uv_net_multi.h"

void on_close(uv_handle_t* handle) {
    client_t* client = (client_t*)handle->data;
    client->is_closing = 1;
    client->response->req_time_end = time(NULL);
}

void safe_close(client_t* client) {
    if (!uv_is_closing((uv_handle_t*)&client->handle)){
        uv_close((uv_handle_t*)&client->timer, NULL);  // Close the timer handle
        uv_close((uv_handle_t*)&client->handle, on_close); // Close the handle
    }
}

void on_timeout(uv_timer_t* timer) {
    client_t* client = (client_t*)timer->data;
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
        // Handle write error
        client->response->status = STATUS_ERROR;
        safe_close(client);
        return;
    }

    // stop write timeout timer
    uv_timer_stop(&client->timer);

    // Restart the read response timeout timer after successfully writing
    uv_timer_start(&client->timer, on_timeout, RESPONSE_TIMEOUT, 0);

    // Start reading from the server
    uv_read_start((uv_stream_t*)req->handle, alloc_buffer, on_read);
}

void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    client_t* client = (client_t*)stream->data;

    if (nread > 0) {
        client->response->data = realloc(client->response->data, client->response->size + nread);
        memcpy(client->response->data + client->response->size, buf->base, nread);
        client->response->size += nread;

        // extend timeout_if_data_received
        if (client->response->size > 0) {
            uv_timer_stop(&client->timer);
            uv_timer_start(&client->timer, on_timeout, RESPONSE_TIMEOUT, 0);
        }

    } else if (nread < 0) {
        if (nread == UV_EOF) {
            client->response->status = STATUS_OK;
        } else {
            client->response->status = STATUS_ERROR;
        }
        uv_timer_stop(&client->timer);
        safe_close(client);
    }

    free(buf->base);
}

void on_connect(uv_connect_t* req, int status) {
    client_t* client = (client_t*)req->data;
    if (client->is_closing || status < 0) {
        // Handle connection error
        DEBUG_PRINT("Connection error %s: %s",client->response->host, uv_strerror(status));
        client->response->status = STATUS_ERROR;
        safe_close(client);
        return;
    }
    // stop connection timeout timer
    uv_timer_stop(&client->timer);
    // Start the timer to wait for write operation
    uv_timer_start(&client->timer, on_timeout, RESPONSE_TIMEOUT, 0);

    // Write the message to the server
//    uv_buf_t buf = uv_buf_init((char*)client->message, strlen(client->message));

//    uv_buf_t buf = uv_buf_init((char *)client->message, strlen(client->message));
    uv_buf_t buf = uv_buf_init((char *)(uintptr_t)client->message, strlen(client->message));
 

    uv_write(&client->write_req, (uv_stream_t*)&client->handle, &buf, 1, on_write);

}

int is_ip_address(const char* host) {
    struct in_addr sa;
    return inet_pton(AF_INET, host, &(sa.s_addr));
}

void start_connection(client_t* client, const struct sockaddr* addr) {
    uv_tcp_connect(&client->connect_req, &client->handle, addr, on_connect);
}

void on_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res) {
    client_t* client = resolver->data;
    if (status == 0) {
        // char ipstr[INET6_ADDRSTRLEN];
        // struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        // inet_ntop(res->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
        // fprintf(stderr, "Resolved IP: %s\n", ipstr);

        if (!client->is_closing) {
            start_connection(client, res->ai_addr);
        }
        uv_freeaddrinfo(res);
    } else {
        // Handle error
        client->response->status = STATUS_ERROR;
    }
    free(resolver);
}

response_t** send_multi_request_old(const char **hosts, int port, const char* message) {
    // count the number of hosts
    int total_hosts = 0;
    while (hosts[total_hosts] != NULL) total_hosts++;
    if (total_hosts == 0)
        return NULL;

    char port_str[6]; //maximum 0..65535 + \0
    sprintf(port_str, "%d", port);

    uv_loop_t* loop = uv_default_loop();

    response_t** responses = calloc(total_hosts + 1, sizeof(response_t*));

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
        uv_timer_start(&client->timer, on_timeout, CONNECTION_TIMEOUT, 0);

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
            // hints.ai_protocol = IPPROTO_TCP;

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

response_t** send_multi_request(const char **hosts, int port, const char* message) {
    // Count the number of hosts
    int total_hosts = 0;
    while (hosts[total_hosts] != NULL) total_hosts++;
    if (total_hosts == 0)
        return NULL;

    char port_str[6]; // Maximum 0..65535 + \0
    sprintf(port_str, "%d", port);

    uv_loop_t* loop = uv_default_loop();
    response_t** responses = calloc(total_hosts + 1, sizeof(response_t*));
    if (!responses) {
        ERROR_PRINT("[ERROR] Failed to allocate memory for responses.");
        return NULL;
    }

    client_t* clients[total_hosts];  // Track client pointers for cleanup

    for (int i = 0; i < total_hosts; i++) {
        clients[i] = NULL;

        if (!hosts[i]) continue;

        // Allocate memory for client_t
        client_t* client = calloc(1, sizeof(client_t));
        if (!client) {
            ERROR_PRINT("[ERROR] Failed to allocate memory for client.");
            continue;
        }

        client->message = strdup(message);  // Use strdup to prevent dangling pointers
        if (!client->message) {
            ERROR_PRINT("[ERROR] Failed to allocate memory for message.");
            free(client);
            continue;
        }

        client->response = calloc(1, sizeof(response_t));
        if (!client->response) {
            ERROR_PRINT("[ERROR] Failed to allocate memory for response.");
            free((char *)client->message);
            free(client);
            continue;
        }

        client->response->host = strdup(hosts[i]);
        if (!client->response->host) {
            ERROR_PRINT("[ERROR] Failed to allocate memory for host.");
            free(client->response);
            free((char *)client->message);
            free(client);
            continue;
        }

        client->response->status = STATUS_PENDING;
        client->response->client = client;
        client->response->req_time_start = time(NULL);
        responses[i] = client->response;
        clients[i] = client;

        if (uv_tcp_init(loop, &client->handle) != 0) {
            ERROR_PRINT("[ERROR] Failed to initialize TCP handle.");
            free(client->response->host);
            free(client->response);
            free((char *)client->message);
            free(client);
            continue;
        }

        if (uv_timer_init(loop, &client->timer) != 0) {
            ERROR_PRINT("[ERROR] Failed to initialize timer handle.");
            free(client->response->host);
            free(client->response);
            free((char *)client->message);
            free(client);
            continue;
        }

        client->handle.data = client;
        client->timer.data = client;
        uv_timer_start(&client->timer, on_timeout, CONNECTION_TIMEOUT, 0);

        if (is_ip_address(hosts[i])) {
            struct sockaddr_in dest;
            uv_ip4_addr(hosts[i], port, &dest);
            start_connection(client, (const struct sockaddr*)&dest);
        } else {
            uv_getaddrinfo_t* resolver = malloc(sizeof(uv_getaddrinfo_t));
            if (!resolver) {
                ERROR_PRINT("[ERROR] Failed to allocate memory for resolver.");
                free(client->response->host);
                free(client->response);
                free((char *)client->message);
                free(client);
                continue;
            }

            struct addrinfo hints = {0};
            hints.ai_family = PF_INET;
            hints.ai_socktype = SOCK_STREAM;

            resolver->data = client;
            if (uv_getaddrinfo(loop, resolver, on_resolved, hosts[i], port_str, &hints) != 0) {
                ERROR_PRINT("[ERROR] uv_getaddrinfo failed.");
                free(resolver);
                free(client->response->host);
                free(client->response);
                free((char *)client->message);
                free(client);
                continue;
            }
        }
    }

    uv_run(loop, UV_RUN_DEFAULT);

    // Cleanup
    for (int i = 0; i < total_hosts; i++) {
        if (clients[i]) {
            safe_close(clients[i]);
        }
    }

    uv_run(loop, UV_RUN_NOWAIT);
    uv_run(loop, UV_RUN_DEFAULT);  // Ensure all handles are closed

    for (int i = 0; i < total_hosts; i++) {
        if (clients[i]) {
            free(clients[i]->response->host);
            free(clients[i]->response->data);
            free(clients[i]->response);
            free((char *)clients[i]->message);
            free(clients[i]);
        }
    }

    uv_loop_close(loop);
    return responses;
}