#ifndef XCASH_MESSAGE_H
#define XCASH_MESSAGE_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "structures.h"
#include "uv_net_server.h"
#include "server_functions.h"
#include "network_security_functions.h"
#include "block_verifiers_synchronize_server_functions.h"
#include "block_verifiers_server_functions.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

bool is_unsigned_type(xcash_msg_t msg);
bool is_walletsign_type(xcash_msg_t msg);
bool is_nonreturn_type(xcash_msg_t msg);
char* create_message_param_list(xcash_msg_t msg, const char** pair_params);
char* create_message(xcash_msg_t msg);
char* create_message_args(xcash_msg_t msg, va_list args);
char* create_message_param(xcash_msg_t msg, ...);
xcash_msg_t get_message_type(const char* data);

// message format helpers
int split(const char* str, char delimiter, char*** result_elements);
void cleanup_char_list(char** element_list);

void handle_srv_message(const char *data, size_t length, server_client_t* client);

#endif  // XCASH_MESSAGE_H