#ifndef XCASH_MESSAGE_H
#define XCASH_MESSAGE_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include <jansson.h>
#include "globals.h"
#include "macro_functions.h"
#include "uv_net_server.h"
#include "server_functions.h"
#include "network_security_functions.h"
#include "block_verifiers_synchronize_server_functions.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
/*
typedef enum {
    XMSG_NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF,
    XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE,
    XMSG_NODE_TO_NETWORK_DATA_NODES_CHECK_VOTE_STATUS,
    XMSG_NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_RECOVER_DELEGATE,
    XMSG_NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH,
    XMSG_BLOCK_VERIFIERS_TO_NODES_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD,
    XMSG_GET_CURRENT_BLOCK_HEIGHT,
    XMSG_SEND_CURRENT_BLOCK_HEIGHT,
    XMSG_MAIN_NODES_TO_NODES_PART_4_OF_ROUND_CREATE_NEW_BLOCK,
    XMSG_MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_START_BLOCK,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA,
    XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS,
    XMSG_NODES_TO_NODES_VOTE_RESULTS,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_INVALID_RESERVE_PROOFS,
    XMSG_NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST,
    XMSG_NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST,
    XMSG_NETWORK_DATA_NODE_TO_NODE_SEND_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST,
    XMSG_NETWORK_DATA_NODE_TO_NODE_SEND_CURRENT_BLOCK_VERIFIERS_LIST,
    XMSG_BLOCK_VERIFIERS_TO_NETWORK_DATA_NODE_BLOCK_VERIFIERS_CURRENT_TIME,
    XMSG_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_BLOCK_VERIFIERS_CURRENT_TIME,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_ONLINE_STATUS,
    XMSG_NODE_TO_BLOCK_VERIFIERS_CHECK_IF_CURRENT_BLOCK_VERIFIER,
    XMSG_BLOCK_VERIFIERS_TO_NODE_SEND_RESERVE_BYTES,
    XMSG_NETWORK_DATA_NODES_TO_NETWORK_DATA_NODES_DATABASE_SYNC_CHECK,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_DOWNLOAD,      // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_DOWNLOAD,  // server answer
    XMSG_XCASH_GET_SYNC_INFO, // get server hashes and block_height
    XMSG_XCASH_GET_BLOCK_PRODUCERS,
    XMSG_XCASH_GET_BLOCK_HASH, 
    XMSG_NODES_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE, 
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_UPDATE,
    XMSG_MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_CREATE_NEW_BLOCK,
    XMSG_MESSAGES_COUNT,
    XMSG_NONE = XMSG_MESSAGES_COUNT
} xcash_msg_t;
*/
extern const char* xcash_net_messages[];
extern const xcash_msg_t xcash_db_sync_messages[];
extern const xcash_msg_t xcash_db_download_messages[];

bool is_unsigned_type(xcash_msg_t msg);
bool is_walletsign_type(xcash_msg_t msg);
bool is_nonreturn_type(xcash_msg_t msg);
char* create_message_param_list(xcash_msg_t msg, const char** pair_params);
char* create_message(xcash_msg_t msg);
char* create_message_args(xcash_msg_t msg, va_list args);
char* create_message_param(xcash_msg_t msg, ...);

// message format helpers
int split(const char* str, char delimiter, char*** result_elements);
void cleanup_char_list(char** element_list);

void handle_srv_message(const char *data, size_t length, server_client_t* client);

#endif  // XCASH_MESSAGE_H