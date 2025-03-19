#include "block_verifiers_update_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: get_block_verifiers_from_network_block
Description: Gets the block verifiers from the network block
Parameters:
  TOTAL_DELEGATES - The total delegates
  delegates - struct delegates
  CURRENT_BLOCK_HEIGHT - The block height
  SETTINGS - 0 for current block verifiers, 1 for previous block verifiers
Return: 0 if an error has occurred, 1 if successful
---------------------------------------------------------------------------------------------------------*/
int get_block_verifiers_from_network_block(const int TOTAL_DELEGATES, const delegates_t* delegates, const size_t CURRENT_BLOCK_HEIGHT, const int SETTINGS) {
  char data[BUFFER_SIZE] = {0};
  char data2[BUFFER_SIZE] = {0};
  char data3[BUFFER_SIZE] = {0};
  char message[BUFFER_SIZE] = {0};
  char* message_copy;
  int count, count2;
  size_t block_height;

  // Determine block height to fetch
  block_height = SETTINGS == 0 ? CURRENT_BLOCK_HEIGHT - 1 : CURRENT_BLOCK_HEIGHT - 2;

  // Prepare query message
  snprintf(data, sizeof(data), "{\"block_height\":\"%zu\"}", block_height);

  // Calculate database reserve bytes collection name
  size_t reserve_block_height = (block_height - 1 == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT || block_height == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT)
                                    ? 1
                                    : ((block_height - XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) / BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME) + 1;

  snprintf(data3, sizeof(data3), "reserve_bytes_%zu", reserve_block_height);

  // Fetch reserve bytes
  if (read_document_field_from_collection(DATABASE_NAME, data3, data, "reserve_bytes", message) == 0) {
    ERROR_PRINT("Can't read from DB %s, searching for block_height %zu", data3, CURRENT_BLOCK_HEIGHT);
    return XCASH_ERROR;
  }

  // Parse reserve bytes message
  message_copy = strstr(message, BLOCKCHAIN_DATA_SEGMENT_PUBLIC_ADDRESS_STRING_DATA);
  if (!message_copy) {
    ERROR_PRINT("Failed to find public address segment in reserve bytes");
    return XCASH_ERROR;
  }
  message_copy += (sizeof(BLOCKCHAIN_DATA_SEGMENT_PUBLIC_ADDRESS_STRING_DATA) - 1);

  // Fill public keys from DB
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    memset(data, 0, sizeof(data));
    memset(data3, 0, sizeof(data3));

    memcpy(data, message_copy, VRF_PUBLIC_KEY_LENGTH * 2);
    message_copy += (VRF_PUBLIC_KEY_LENGTH * 2) + (sizeof(BLOCKCHAIN_DATA_SEGMENT_PUBLIC_ADDRESS_STRING_DATA) - 1);

    // Convert hex string to binary
    for (int j = 0; j < VRF_PUBLIC_KEY_LENGTH; j++) {
      char byte_str[3] = {data[j * 2], data[j * 2 + 1], 0};
      data3[j] = (char)strtol(byte_str, NULL, 16);
    }

    // Copy to correct verifier list
    if (SETTINGS == 0) {
      memcpy(current_block_verifiers_list.block_verifiers_public_key[count], data3, VRF_PUBLIC_KEY_LENGTH);
    } else {
      memcpy(previous_block_verifiers_list.block_verifiers_public_key[count], data3, VRF_PUBLIC_KEY_LENGTH);
    }
  }

  // Fill rest of verifier details from delegates
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    for (count2 = 0; count2 < TOTAL_DELEGATES; count2++) {
      int match = (SETTINGS == 0)
                      ? strncmp(current_block_verifiers_list.block_verifiers_public_key[count], delegates[count2].public_key, VRF_PUBLIC_KEY_LENGTH)
                      : strncmp(previous_block_verifiers_list.block_verifiers_public_key[count], delegates[count2].public_key, VRF_PUBLIC_KEY_LENGTH);

      if (match == 0) {
        block_verifiers_list_t* target_list = (SETTINGS == 0) ? &current_block_verifiers_list : &previous_block_verifiers_list;
        memcpy(target_list->block_verifiers_name[count], delegates[count2].delegate_name,
               strnlen(delegates[count2].delegate_name, sizeof(target_list->block_verifiers_name[count])));
        memcpy(target_list->block_verifiers_IP_address[count], delegates[count2].IP_address,
               strnlen(delegates[count2].IP_address, sizeof(target_list->block_verifiers_IP_address[count])));
        memcpy(target_list->block_verifiers_public_address[count], delegates[count2].public_address,
               strnlen(delegates[count2].public_address, sizeof(target_list->block_verifiers_public_address[count])));
        break;
      }
    }
  }

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: update_block_verifiers_list
Description: Updates the block verifiers list struct
Return: 0 if an error has occurred, 1 to sync from a random block verifier, 2 to sync from a random network data node
---------------------------------------------------------------------------------------------------------*/
int update_block_verifiers_list(void) {
  int count, count2;
  int settings = 0;
  size_t total_delegates = 0;
  size_t current_block_height_count;
  delegates_t* delegates = calloc(MAXIMUM_AMOUNT_OF_DELEGATES, sizeof(delegates_t));
  if (!delegates) {
    ERROR_PRINT("Memory allocation failed");
    return 0;
  }

  if (read_organize_delegates(delegates, &total_delegates) != XCASH_OK) {
    ERROR_PRINT("Could not organize the delegates");
    free(delegates);
    return 0;
  }

  total_delegates = total_delegates > BLOCK_VERIFIERS_TOTAL_AMOUNT ? BLOCK_VERIFIERS_TOTAL_AMOUNT : total_delegates;
  sscanf(current_block_height, "%zu", &current_block_height_count);

  // Helper functions
  void clear_verifier_lists(block_verifiers_list_t * list) {
    for (int i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
      memset(list->block_verifiers_name[i], 0, sizeof(list->block_verifiers_name[i]));
      memset(list->block_verifiers_public_address[i], 0, sizeof(list->block_verifiers_public_address[i]));
      memset(list->block_verifiers_public_key[i], 0, sizeof(list->block_verifiers_public_key[i]));
      memset(list->block_verifiers_IP_address[i], 0, sizeof(list->block_verifiers_IP_address[i]));
    }
  }

  void copy_delegate_to_list(delegates_t * delegates, block_verifiers_list_t * list, int count) {
    memcpy(list->block_verifiers_name[count], delegates[count].delegate_name,
           strnlen(delegates[count].delegate_name, sizeof(list->block_verifiers_name[count])));
    memcpy(list->block_verifiers_public_address[count], delegates[count].public_address,
           strnlen(delegates[count].public_address, sizeof(list->block_verifiers_public_address[count])));
    memcpy(list->block_verifiers_public_key[count], delegates[count].public_key,
           strnlen(delegates[count].public_key, sizeof(list->block_verifiers_public_key[count])));
    memcpy(list->block_verifiers_IP_address[count], delegates[count].IP_address,
           strnlen(delegates[count].IP_address, sizeof(list->block_verifiers_IP_address[count])));
  }

  if (current_block_height_count <= XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
    // No previous blocks
    clear_verifier_lists(&previous_block_verifiers_list);
    clear_verifier_lists(&current_block_verifiers_list);
    clear_verifier_lists(&next_block_verifiers_list);

    for (count = 0; count < (int)total_delegates; count++) {
      copy_delegate_to_list(delegates, &previous_block_verifiers_list, count);
      copy_delegate_to_list(delegates, &current_block_verifiers_list, count);
      copy_delegate_to_list(delegates, &next_block_verifiers_list, count);
    }
  } else if (current_block_height_count == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT + 1) {
    // Load next block verifiers
    clear_verifier_lists(&next_block_verifiers_list);
    for (count = 0; count < BLOCK_VERIFIERS_TOTAL_AMOUNT; count++) {
      copy_delegate_to_list(delegates, &next_block_verifiers_list, count);
    }

    // Copy current to previous
    clear_verifier_lists(&previous_block_verifiers_list);
    for (count = 0; count < BLOCK_VERIFIERS_TOTAL_AMOUNT; count++) {
      memcpy(previous_block_verifiers_list.block_verifiers_name[count],
             current_block_verifiers_list.block_verifiers_name[count],
             strnlen(current_block_verifiers_list.block_verifiers_name[count],
                     sizeof(previous_block_verifiers_list.block_verifiers_name[count])));

      memcpy(previous_block_verifiers_list.block_verifiers_public_address[count],
             current_block_verifiers_list.block_verifiers_public_address[count],
             strnlen(current_block_verifiers_list.block_verifiers_public_address[count],
                     sizeof(previous_block_verifiers_list.block_verifiers_public_address[count])));

      memcpy(previous_block_verifiers_list.block_verifiers_public_key[count],
             current_block_verifiers_list.block_verifiers_public_key[count],
             strnlen(current_block_verifiers_list.block_verifiers_public_key[count],
                     sizeof(previous_block_verifiers_list.block_verifiers_public_key[count])));

      memcpy(previous_block_verifiers_list.block_verifiers_IP_address[count],
             current_block_verifiers_list.block_verifiers_IP_address[count],
             strnlen(current_block_verifiers_list.block_verifiers_IP_address[count],
                     sizeof(previous_block_verifiers_list.block_verifiers_IP_address[count])));
    }

    clear_verifier_lists(&current_block_verifiers_list);

    if (get_block_verifiers_from_network_block(total_delegates, delegates, current_block_height_count, 0) == 0) {
      ERROR_PRINT("Could not get current block verifiers from network block");
      free(delegates);
      return 0;
    }
  } else {
    // Load next block verifiers
    clear_verifier_lists(&next_block_verifiers_list);
    for (count = 0; count < BLOCK_VERIFIERS_TOTAL_AMOUNT; count++) {
      copy_delegate_to_list(delegates, &next_block_verifiers_list, count);
    }

    clear_verifier_lists(&current_block_verifiers_list);
    if (get_block_verifiers_from_network_block(total_delegates, delegates, current_block_height_count, 0) == 0) {
      ERROR_PRINT("Could not get current block verifiers from network block");
      free(delegates);
      return 0;
    }

    clear_verifier_lists(&previous_block_verifiers_list);
    if (get_block_verifiers_from_network_block(total_delegates, delegates, current_block_height_count, 1) == 0) {
      ERROR_PRINT("Could not get previous block verifiers from network block");
      free(delegates);
      return 0;
    }
  }

  // Check overlap between current and previous verifiers
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    for (count2 = 0; count2 < BLOCK_VERIFIERS_AMOUNT; count2++) {
      if (strncmp(previous_block_verifiers_list.block_verifiers_public_address[count],
                  current_block_verifiers_list.block_verifiers_public_address[count2],
                  XCASH_WALLET_LENGTH) == 0) {
        settings++;
        break;
      }
    }
  }
  settings = settings > (BLOCK_VERIFIERS_AMOUNT - BLOCK_VERIFIERS_VALID_AMOUNT) ? 1 : 2;

  free(delegates);
  return settings;
}