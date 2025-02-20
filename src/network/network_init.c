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

#define BACKLOG_SIZE 128
#define DEFAULT_PORT 18281

int server_socket = -1;
int epoll_fd = -1;

bool create_server(void)
{
    struct sockaddr_in6 addr;
    struct epoll_event event;
    int optval = 0;  // Set to 0 to allow both IPv4 and IPv6

    DEBUG_PRINT("Creating the server");

    // Create a non-blocking IPv6+IPv4 socket
    if ((server_socket = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
    {
        DEBUG_PRINT("Can't create the socket");
        return false;
    }

    // Enable IPv6-only mode if required (0 allows both IPv4 & IPv6)
    if (setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval)) < 0)
    {
        DEBUG_PRINT("Can't set IPv6 dual-stack mode");
        close(server_socket);
        return false;
    }

    // Allow address/port reuse for quick restart
    optval = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval)) != 0)
    {
        DEBUG_PRINT("Can't set the socket options");
        close(server_socket);
        return false;
    }

    // Enable TCP keepalive for stable P2P connections
    if (setsockopt(server_socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) != 0)
    {
        DEBUG_PRINT("Can't enable TCP keepalive");
        close(server_socket);
        return false;
    }

    memset(&addr, 0, sizeof(addr));

    // Setup the P2P connection
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(XCASH_DPOPS_PORT);

    // Ensure `XCASH_DPOPS_delegates_IP_address` is not null
    if (XCASH_DPOPS_delegates_IP_address == NULL || strlen(XCASH_DPOPS_delegates_IP_address) == 0)
    {
        DEBUG_PRINT("Invalid delegate IP address");
        close(server_socket);
        return false;
    }

    // Correct string comparison
    if (strcmp(XCASH_DPOPS_delegates_IP_address, "127.0.0.1") == 0)
    {
        addr.sin6_addr = in6addr_any;
    }
    else
    {
        if (inet_pton(AF_INET6, XCASH_DPOPS_delegates_IP_address, &addr.sin6_addr) != 1)
        {
            DEBUG_PRINT("Invalid IPv6 address format: %s", XCASH_DPOPS_delegates_IP_address);
            close(server_socket);
            return false;
        }
    }

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        DEBUG_PRINT("Can't bind to server socket on port %d", XCASH_DPOPS_PORT);
        close(server_socket);
        return false;
    }

    // Listen with an optimized backlog queue
    if (listen(server_socket, BACKLOG_SIZE) != 0)
    {
        DEBUG_PRINT("Can't start listening");
        close(server_socket);
        return false;
    }

    DEBUG_PRINT("Started service on port %d", XCASH_DPOPS_PORT);

    // Set the server socket to non-blocking mode (Check fcntl return value)
    int flags = fcntl(server_socket, F_GETFL, 0);
    if (flags == -1 || fcntl(server_socket, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        DEBUG_PRINT("Failed to set non-blocking mode");
        close(server_socket);
        return false;
    }

    // Create epoll for efficient event handling
    if ((epoll_fd = epoll_create1(0)) == -1)
    {
        DEBUG_PRINT("Can't start epoll");
        close(server_socket);
        return false;
    }

    // Add the server socket to epoll for monitoring incoming connections
    event.events = EPOLLIN | EPOLLET;  // Use edge-triggered mode for better performance
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        DEBUG_PRINT("Can't add server socket to epoll");
        close(server_socket);
        close(epoll_fd);
        epoll_fd = -1;  // Prevents double-close errors
        return false;
    }

    return true;
}