#ifndef XCASH_NET_H
#define XCASH_NET_H

#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#include "xcash_message.h"
#include "config.h"
#include "globals.h"
#include "network_functions.h"
#include "uv_net_multi.h"

typedef enum XNET_DEST{
    XNET_SEEDS_ALL,
    XNET_SEEDS_ALL_ONLINE,
    XNET_DELEGATES_ALL,
    XNET_DELEGATES_ALL_ONLINE,

    XNET_NONE
} xcash_dest_t;

// Remove trailing "|END|" from each valid response in the array.
void remove_enders(response_t **responses);

// Sends a message (appending "|END|") to a predefined group of nodes.
bool xnet_send_data_multi(xcash_dest_t dest, const char* message, response_t ***reply);

// Sends a message with parameter list to a group of nodes.
bool send_message_param_list(xcash_dest_t dest, xcash_msg_t msg, response_t ***reply, const char** pair_params);

// Sends a message with variadic parameters to a group of nodes.
bool send_message_param(xcash_dest_t dest, xcash_msg_t msg, response_t ***reply, ...);

// Sends a basic message with no parameters.
bool send_message(xcash_dest_t dest, xcash_msg_t msg, response_t ***reply);

// Sends a message with parameter list directly to a single host.
bool send_direct_message_param_list(const char* host, xcash_msg_t msg, response_t ***reply, const char** pair_params);

// Sends a message with variadic parameters directly to a single host.
bool send_direct_message_param(const char* host, xcash_msg_t msg, response_t ***reply, ...);

// Sends a basic message with no parameters to a single host.
bool send_direct_message(const char* host, xcash_msg_t msg, response_t ***reply);

#endif // XCASH_NET_H