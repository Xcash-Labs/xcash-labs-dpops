#include "node_functions.h"

bool get_node_data(void) {
    // Get the wallet's public address
    if (!get_public_address()) {
        DEBUG_PRINT("Could not get the wallet's public address");
        return XCASH_ERROR;
    }

    if (xcash_wallet_public_address[0] == '\0') {
        DEBUG_PRINT("Wallet public address is empty");
        return XCASH_ERROR;
    }

    is_seed_node = is_seed_address(xcash_wallet_public_address);
    return true;
}

bool is_seed_address(const char* public_address) {
    for (size_t i = 0; network_nodes[i].seed_public_address != NULL; i++) {
        if (strcmp(network_nodes[i].seed_public_address, public_address) == 0) {
            return true;
        }
    }
    return false;
}