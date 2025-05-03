#ifndef XCASH_NET_H
#define XCASH_NET_H

#include "xcash_message.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
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


bool xnet_send_data_multi(xcash_dest_t dest, const char* message, response_t ***reply);
bool send_message_param_list(xcash_dest_t dest, xcash_msg_t msg, response_t ***reply, const char** pair_params);
bool send_message_param(xcash_dest_t dest, xcash_msg_t msg, response_t ***reply, ...);
bool send_message(xcash_dest_t dest, xcash_msg_t msg, response_t ***reply);
bool send_direct_message_param_list(const char* host, xcash_msg_t msg, response_t ***reply, const char** pair_params);
bool send_direct_message_param(const char* host, xcash_msg_t msg, response_t ***reply, ...);
bool send_direct_message(const char* host, xcash_msg_t msg, response_t ***reply);

#endif // XCASH_NET_H