#ifndef NET_SERVER_H
#define NET_SERVER_H

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
    int socket_fd;
    char client_ip[INET_ADDRSTRLEN];
} server_client_t;

 
#include "xcash_message.h"

void* server_thread_loop(void* arg);
void* handle_client(void* client_socket_ptr);
int start_tcp_server(int port);
void stop_tcp_server(void);

#endif