#include "node_functions.h"

bool is_seed_address(const char* public_address) {
    if (!public_address) {
        return false;
    }
    for (size_t i = 0; network_data_nodes_list.network_data_nodes_public_address[i] != NULL; i++) {
        if (strcmp(network_data_nodes_list.network_data_nodes_public_address[i], public_address) == 0) {
            return XCASH_OK;
        }
    }
    return false;
}