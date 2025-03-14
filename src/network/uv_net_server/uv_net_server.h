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
    char client_ip[INET6_ADDRSTRLEN];
} server_client_t;

#include "xcash_message.h"

void on_new_connection(uv_stream_t *server_handle, int status);
void alloc_buffer_srv(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void on_client_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf);
bool start_tcp_server(int port);
void stop_tcp_server(void);
void send_response(server_client_t *client, const char *message);

#endif