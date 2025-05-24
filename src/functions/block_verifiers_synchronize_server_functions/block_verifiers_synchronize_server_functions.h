#ifndef BLOCK_VERIFIERS_SYNCHRONIZE_SERVER_FUNCTIONS_H_   /* Include guard */
#define BLOCK_VERIFIERS_SYNCHRONIZE_SERVER_FUNCTIONS_H_

#include "net_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "db_operations.h"
#include "db_sync.h"
#include "xcash_message.h"

void server_received_msg_get_sync_info(server_client_t* client, const char* MESSAGE);

#endif