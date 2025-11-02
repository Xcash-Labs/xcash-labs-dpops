#ifndef SERVER_FUNCTIONS_H_   /* Include guard */
#define SERVER_FUNCTIONS_H_

#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 
#include <pthread.h>
#include <stdbool.h>
#include "config.h"
#include "globals.h"
#include "string_functions.h"
#include "network_security_functions.h"

int server_limit_IP_addresses(limit_action_t action, const char* IP_ADDRESS);
int server_limit_public_addresses(limit_action_t action, const char* MESSAGE);
bool get_self_sha256(char out_hex[SHA256_DIGEST_SIZE + 1]);

#endif