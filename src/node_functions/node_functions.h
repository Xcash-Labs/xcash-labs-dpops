#ifndef NODE_FUNCTIONS_H
#define NODE_FUNCTIONS_H

#include <stdbool.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "network_wallet_functions.h"
#include "structures.h"
#include "network_daemon_functions.h"
#include "db_functions.h"

bool get_node_data(void);
bool is_seed_address(const char* public_address);
int get_seed_node_count(void);
void get_vrf_public_key(void);
const char* address_to_node_name(const char* public_address);
const char* address_to_node_host(const char* public_address);

#endif  // NODE_FUNCTIONS_H