#include "node_functions.h"

bool get_node_data(void) {
  // Get the wallet's public address
  const int MAX_WAIT_SEC = 300;
  const int SLEEP_SEC = 5;
  time_t t0 = time(NULL);
  int attempt = 0;

  while (!get_public_address()) {
    if (difftime(time(NULL), t0) >= MAX_WAIT_SEC) {
      FATAL_ERROR_EXIT("Could not get the wallet's public address within %d seconds", MAX_WAIT_SEC);
      return XCASH_ERROR;
    }
    attempt++;
    WARNING_PRINT("Wallet not ready yet (attempt %d). Retrying in %ds...", attempt, SLEEP_SEC);
    sleep(SLEEP_SEC);
  }

  if (xcash_wallet_public_address[0] == '\0') {
    ERROR_PRINT("Wallet public address is empty");
    return XCASH_ERROR;
  }

  get_vrf_public_key();
  if (vrf_public_key[0] == '\0') {
    WARNING_PRINT("Failed to read vrf_public_key for delegate, has this delegate been registered?");
  }
  return XCASH_OK;
}

bool is_seed_address(const char *public_address) {
  for (size_t i = 0; network_nodes[i].seed_public_address != NULL; i++) {
    if (strcmp(network_nodes[i].seed_public_address, public_address) == 0) {
      return true;
    }
  }
  return false;
}


void get_vrf_public_key() {
  char filter_json[256] = {0};

  snprintf(filter_json, sizeof(filter_json), "{ \"public_address\": \"%s\" }", xcash_wallet_public_address);

  if (read_document_field_from_collection(
        DATABASE_NAME,
        DB_COLLECTION_DELEGATES,
        filter_json,
        "public_key",
        vrf_public_key,
        sizeof(vrf_public_key)) != XCASH_OK)
  {
    memset(vrf_public_key, 0, sizeof(vrf_public_key));
  }
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
bool get_reserve_bytes_database(size_t *count) {
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