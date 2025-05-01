#ifndef UV_NET_SERVER_H
#define UV_NET_SERVER_H

#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <pthread.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"

typedef struct {
    uv_tcp_t handle;
    char *data;
    size_t data_size;
    char client_ip[INET6_ADDRSTRLEN];
    bool sent_reply;
    bool received_reply;
    bool write_timeout;
    bool closed; 
} server_client_t;

typedef struct {
    uv_write_t req;
    uv_timer_t timer;
    char *message_copy;
    server_client_t *client;
} write_srv_request_t;

typedef struct {
    server_client_t *client;
    char *data;
    size_t data_len;
} message_work_t;

#include "xcash_message.h"

void on_new_connection(uv_stream_t *server_handle, int status);
void alloc_buffer_srv(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void on_client_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf);
bool start_tcp_server(int port);
void stop_tcp_server(void);
void send_data_uv(server_client_t *client, const char *message);
void on_write_complete(uv_write_t *req, int status);

#endif