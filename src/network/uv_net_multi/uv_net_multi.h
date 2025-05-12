#ifndef UV_NET_H
#define UV_NET_H

#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "macro_functions.h"



typedef enum {
    STATUS_ERROR,
    STATUS_OK,
    STATUS_PENDING,
    STATUS_TIMEOUT,
    STATUS_INCOMPLETE,
} response_status_t;


typedef struct client_t client_t;

typedef struct {
    char *host;
    char *data;
    size_t size;
    time_t req_time_start; // timestamp before connection start
    time_t req_time_end; // timestamp after connection closed
    response_status_t status;
    client_t *client;
} response_t;


struct client_t{
    uv_tcp_t handle;
    uv_connect_t connect_req;
    uv_write_t write_req;
    uv_timer_t timer;
    int is_closing;
    response_t *response;
    const char *message;
};

void on_close(uv_handle_t * handle);

void on_timeout(uv_timer_t *timer);

void on_write(uv_write_t *req, int status);

void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

void on_connect(uv_connect_t *req, int status);

response_t **send_multi_request(const char **hosts, int port, const char *message);

void cleanup_responses(response_t **responses);

#endif // UV_NET_H