#ifndef UV_NET_MULTI_H
#define UV_NET_MULTI_H

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <uv.h>
#include <time.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"

typedef enum {
    STATUS_PENDING,
    STATUS_OK,
    STATUS_ERROR,
    STATUS_TIMEOUT
} response_status_t;

// Client structure for handling network requests
typedef struct {
    uv_tcp_t handle;
    uv_connect_t connect_req;
    uv_timer_t timer;
    uv_write_t write_req;
    struct response_t *response;
    const char *message;
    int retry_count;  // Number of retry attempts
    int is_closing;
} client_t;

// Response structure for handling server responses
typedef struct response_t {
    char *host;
    char *data;
    size_t size;
    response_status_t status;
    client_t *client;
    time_t req_time_start;
    time_t req_time_end;
} response_t;

// Function prototypes
void safe_close(client_t* client);
void on_timeout(uv_timer_t* timer);
void alloc_buffer_multi(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
void on_write(uv_write_t* req, int status);
void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
void retry_connection(uv_timer_t* timer);
void on_connect(uv_connect_t* req, int status);
void start_connection(client_t* client, const struct sockaddr* addr);
void on_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res);
response_t** send_multi_request(const char **hosts, int port, const char* message);

#endif