#ifndef UV_NET_SERVER_H
#define UV_NET_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"

typedef struct {
    char client_ip[INET6_ADDRSTRLEN];
    bool sent_reply;
    bool received_reply;
    bool closed;
    char *buffer;
    size_t buffer_size;
} server_client_t;

 
#include "xcash_message.h"

void* server_thread_loop(void* arg);
void* handle_client(void* client_socket_ptr);
int start_tcp_server(int port);

#endif