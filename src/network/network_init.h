#ifndef SERVER_H_
#define SERVER_H_

#include <stdbool.h>
#include <netinet/in.h>  // For sockaddr_in and sockaddr_in6

// Global Variables
extern int server_socket;
extern int epoll_fd;

// Function Prototypes
bool create_server(void);

//void start_epoll_event_loop(void);
//void accept_new_connection(void);
//void handle_client_message(int client_fd);
//void set_nonblocking(int sock);

#endif /* SERVER_H_ */
