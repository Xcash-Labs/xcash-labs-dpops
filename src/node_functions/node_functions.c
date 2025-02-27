#include "node_functions.h"

bool get_node_data(void) {
    // Get the wallet's public address
    if (!get_public_address()) {
        DEBUG_PRINT("Could not get the wallet's public address");
        return XCASH_ERROR
    }

    if (xcash_wallet_public_address[0] == '\0') {
        DEBUG_PRINT("Wallet public address is empty");
        return XCASH_ERROR
    }

    is_seed_node = is_seed_address(xcash_wallet_public_address);
    network_data_node_settings = is_seed_node ? 1 : 0;

    return true;
}

int is_seed_address(const char* public_address) {
    if (!public_address) {
        return false;
    }
    for (size_t i = 0; network_nodes.seed_public_address[i] != NULL; i++) {
        if (strcmp(network_nodes.seed_public_address[i], public_address) == 0) {
            return true
        }
    }
    return false;
}