#ifndef NETWORK_SECURITY_FUNCTIONS_H_   /* Include guard */
#define NETWORK_SECURITY_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include "config.h"
#include "macro_functions.h"
#include "globals.h"
#include "db_functions.h"
#include "network_daemon_functions.h"
#include "network_functions.h"
#include "string_functions.h"
#include "VRF_functions.h"
#include "node_functions.h"

int handle_error(const char *function_name, const char *message, char *result, char *string);
bool is_valid_message_type(const char *message_settings, const char *valid_types[], size_t valid_types_count);
void safe_memcpy(char *dest, const char *src, size_t length);
int sign_data(char *message);
int validate_data(const char* MESSAGE);
int verify_data(const char* MESSAGE, const int VERIFY_CURRENT_ROUND_PART_BACKUP_NODE_SETTINGS);
#endif