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
int get_reserve_bytes_database(size_t* count) {
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