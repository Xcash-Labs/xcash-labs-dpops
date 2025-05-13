#include "node_functions.h"

bool get_node_data(void) {
    // Get the wallet's public address
    if (!get_public_address()) {
        ERROR_PRINT("Could not get the wallet's public address");
        return XCASH_ERROR;
    }

    if (xcash_wallet_public_address[0] == '\0') {
        ERROR_PRINT("Wallet public address is empty");
        return XCASH_ERROR;
    }

    is_seed_node = is_seed_address(xcash_wallet_public_address);

    // Match by public_address
    char filter_json[256];
    snprintf(filter_json, sizeof(filter_json),
             "{ \"public_address\": \"%s\" }", xcash_wallet_public_address);
    if (read_document_field_from_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, filter_json, "public_key", vrf_public_key) == XCASH_OK) {
      DEBUG_PRINT("Delegate Public Key: %s\n", vrf_public_key);
            INFO_PRINT_STATUS_OK("Wallet Public Address: %s\n", xcash_wallet_public_address);
    } else {
      FATAL_ERROR_EXIT("Failed to read public_key from db for delegate");
    }

    return XCASH_OK;
}

bool is_seed_address(const char* public_address) {
    for (size_t i = 0; network_nodes[i].seed_public_address != NULL; i++) {
        if (strcmp(network_nodes[i].seed_public_address, public_address) == 0) {
            return true;
        }
    }
    return false;
}

int get_seed_node_count() {
    int count = 0;
    for (int i = 0; network_nodes[i].seed_public_address != NULL; i++) {
        count++;
    }
    return count;
}

/*-----------------------------------------------------------------------------------------------------------
 * @brief Gets the current or previous reserve bytes database count.
 * 
 * @param count Pointer to store the calculated count.
 * @param settings 0 for current block's reserve bytes, 1 for previous block's.
 * @return int XCASH_OK if successful, XCASH_ERROR if an error occurs.
-----------------------------------------------------------------------------------------------------------*/
bool get_reserve_bytes_database(size_t* count) {
    if (!count) {
      ERROR_PRINT("Invalid argument: count cannot be NULL.");
      return XCASH_ERROR;
    }
  
    size_t block_height;
    if (sscanf(current_block_height, "%zu", &block_height) != 1) {
      ERROR_PRINT("Failed to parse current block height.");
      return XCASH_ERROR;
    }
  
    *count = block_height;
  
    if (*count - 1 <= XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
      *count = 1;
    } else {
      *count = ((*count - XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) / BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME) + 1;
    }
  
    return XCASH_OK;
  }

/**
 * @brief Resolves the host IP address for a given public address using the NetworkNode array.
 * 
 * @param public_address The public address to resolve.
 * @return const char* Returns the IP address as a string if found, NULL otherwise.
 */
const char *address_to_node_host(const char *public_address) {
    if (!public_address) {
        ERROR_PRINT("Invalid public address (NULL pointer).");
        return NULL;
    }

    // Check against network_nodes array
    for (size_t i = 0; network_nodes[i].seed_public_address != NULL; ++i) {
        if (strcmp(network_nodes[i].seed_public_address, public_address) == 0) {
            INFO_PRINT("Found public address in network_nodes array.");
            return network_nodes[i].ip_address;
        }
    }

    // Check against all delegates
    for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; ++i) {
        if (strcmp(delegates_all[i].public_address, public_address) == 0) {
            INFO_PRINT("Found public address in delegates list.");
            return delegates_all[i].IP_address;
        }
    }

    WARNING_PRINT("Public address %s not found in any list.", public_address);
    return NULL;
}

/**
 * @brief Resolves the delegate name or seed name for a given public address.
 * 
 * @param public_address The public address to resolve.
 * @return const char* Returns the delegate name or seed name if found, NULL otherwise.
 */
const char *address_to_node_name(const char *public_address) {
    if (!public_address) {
        ERROR_PRINT("Invalid public address (NULL pointer).");
        return NULL;
    }

    // Check against network_nodes array for seed names
    for (size_t i = 0; network_nodes[i].seed_public_address != NULL; ++i) {
        if (strcmp(network_nodes[i].seed_public_address, public_address) == 0) {
            INFO_PRINT("Found public address in network_nodes array.");
            return network_nodes[i].ip_address;  // Return seed name as IP address
        }
    }

    // Check against all delegates for delegate names
    for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; ++i) {
        if (strcmp(delegates_all[i].public_address, public_address) == 0) {
            INFO_PRINT("Found public address in delegates list.");
            return delegates_all[i].delegate_name;  // Return delegate name
        }
    }

    WARNING_PRINT("Public address %s not found in any list.", public_address);
    return NULL;
}

/**
 * @brief Retrieves daemon data including the current block height and previous block hash.
 * 
 * @return int Returns XCASH_OK (1) if successful, XCASH_ERROR (0) if an error occurs.
 */
bool get_daemon_data(void) {
    // Get the current block height
    if (!get_current_block_height(current_block_height)) {
        ERROR_PRINT("Could not get the current block height.");
        return false;
    }

    // Validate current block height
    long current_height = atol(current_block_height);
    if (current_height <= 0) {
        ERROR_PRINT("Invalid block height retrieved: %s", current_block_height);
        return false;
    }

    if (current_height < XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
        ERROR_PRINT("Current Block Height (%ld) is below DPOPS era. The blockchain data may not be fully synchronized yet.", current_height);
        return false;
    }

    // Get the previous block hash
    if (!get_previous_block_hash(previous_block_hash)) {
        ERROR_PRINT("Could not get the previous block hash.");
        return false;
    }

    // Validate previous block hash
    if (previous_block_hash[0] == '\0') {
        ERROR_PRINT("Previous block hash is empty. Consider going offline to avoid errors.");
        return false;
    }

    return true;
}