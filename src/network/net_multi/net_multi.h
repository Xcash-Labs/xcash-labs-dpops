#ifndef NET_MULTI_H
#define NET_MULTI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h" 
#include "string_functions.h"

typedef enum {
    STATUS_ERROR,
    STATUS_OK,
    STATUS_PENDING,
    STATUS_TIMEOUT,
    STATUS_INCOMPLETE,
} response_status_t;

typedef struct {
  char* host;
  char* data; // will be NULL since no response expected
  time_t req_time_start;
  time_t req_time_end;
  response_status_t status;
  void* client; // optional, will be NULL
} response_t;


typedef struct {
    const char** hosts;
    int port;
    const char* message;
    response_t** results;  // output
  } multi_request_args_t;


response_t **send_multi_request(const char **hosts, int port, const char *message);
void cleanup_responses(response_t **responses);
bool delegate_connect_check(const char* host, int port);

#endif