#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.h"
#include "globals.h"
#include "macro_functions.h"

int server_socket;
int epoll_fd;

bool create_server(void)
{
    struct epoll_event events;
    struct sockaddr_in6 addr; // Use IPv6-compatible structure
    int optval = 1;
    DEBUG_PRINT("Creating the server");
    // Create a non-blocking IPv6+IPv4 socket (dual-stack)
    if ((server_socket = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
    {
        DEBUG_PRINT("Can't create the socket");
        return false;
    }
    // Allow IPv4 and IPv6 on the same socket (P2P-friendly)
    if (setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval)) < 0)
    {
        DEBUG_PRINT("Can't set IPv6 dual-stack mode");
        return false;
    }
    // Allow address/port reuse for quick restart
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval)) != 0)
    {
        DEBUG_PRINT("Can't set the socket options");
        return false;
    }
    // Enable TCP keepalive for stable P2P connections
    if (setsockopt(server_socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) != 0)
    {
        DEBUG_PRINT("Can't enable TCP keepalive");
        return false;
    }
    memset(&addr, 0, sizeof(addr));
    // Setup the P2P connection
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(XCASH_DPOPS_PORT);
    // Bind to all interfaces (IPv4 & IPv6) or specific IP
    if (memcmp(XCASH_DPOPS_delegates_IP_address, "127.0.0.1", 9) == 0)
    {
        addr.sin6_addr = in6addr_any; // Listen on all interfaces
    }
    else
    {
        inet_pton(AF_INET6, XCASH_DPOPS_delegates_IP_address, &addr.sin6_addr);
    }
    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        DEBUG_PRINT("Can't bind to server socket on port %d", XCASH_DPOPS_PORT);
        return false;
    }
    // Listen with an optimized backlog queue
    if (listen(server_socket, BACKLOG_SIZE) != 0)
    {
        DEBUG_PRINT("Can't start listening");
        return false;
    }
    DEBUG_PRINT("Started service on port " BLUE_TEXT("%d"), XCASH_DPOPS_PORT);
    // Create epoll for efficient event handling
    if ((epoll_fd = epoll_create1(0)) == -1)
    {
        DEBUG_PRINT("Can't start epoll");
        return false;
    }
    return true;
}