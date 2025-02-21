#ifndef NETWORK_FUNCTIONS_H_   /* Include guard */
#define NETWORK_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <ctype.h>
#include <arpa/inet.h>
#include "config.h"
#include "globals.h"

int check_if_IP_address_or_hostname(const char* HOST);
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
int send_http_request(char *result, const char *host, const char *url, int port, 
    const char *method, const char **headers, size_t headers_length, 
    const char *data, int timeout);
  
#endif