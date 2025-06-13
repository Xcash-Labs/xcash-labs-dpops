#ifndef NETWORK_SECURITY_FUNCTIONS_H_   /* Include guard */
#define NETWORK_SECURITY_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "config.h"
#include "macro_functions.h"
#include "globals.h"
#include "db_functions.h"
#include "network_daemon_functions.h"
#include "network_functions.h"
#include "string_functions.h"
#include "VRF_functions.h"
#include "node_functions.h"

int handle_error(const char *function_name, const char *message, char *buf1, char *buf2, char *buf3);
int sign_data(char *message);
bool sign_block_blob(const char* block_blob_hex, char* signature_out, size_t sig_out_len);
int verify_action_data(const char *message);
int verify_ip(const char *message, const char *client_ip);

#endif