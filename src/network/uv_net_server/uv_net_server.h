#ifndef UV_NET_SERVER_H
#define UV_NET_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void on_client_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf);

#endif