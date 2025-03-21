#include "block_verifiers_functions.h"

int block_verifiers_create_block_signature(char* message)
{
  char data[BUFFER_SIZE];
  size_t count, count2, counter;
  int block_producer_backup_settings[5] = {0};  // 5 backups

  const char* backup_public_addresses[] = {
    main_nodes_list.block_producer_public_address,
    main_nodes_list.block_producer_backup_block_verifier_1_public_address,
    main_nodes_list.block_producer_backup_block_verifier_2_public_address,
    main_nodes_list.block_producer_backup_block_verifier_3_public_address,
    main_nodes_list.block_producer_backup_block_verifier_4_public_address,
    main_nodes_list.block_producer_backup_block_verifier_5_public_address
  };

  memset(data, 0, sizeof(data));

  // Convert network block string to blockchain data
  if (network_block_string_to_blockchain_data(VRF_data.block_blob, "0", BLOCK_VERIFIERS_AMOUNT) == 0) {
    ERROR_PRINT("Could not convert the network block string to a blockchain data");
    return XCASH_ERROR;
  }

  // Set block producer network block nonce
  memcpy(blockchain_data.nonce_data, BLOCK_PRODUCER_NETWORK_BLOCK_NONCE, sizeof(BLOCK_PRODUCER_NETWORK_BLOCK_NONCE) - 1);

  // Determine current block producer or backup node
  int backup_index = current_round_part_backup_node[0] - '0'; // Converts "0"-"5" to integer

  if (backup_index >= 0 && backup_index <= 5) {
    for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count],
                  backup_public_addresses[backup_index], XCASH_WALLET_LENGTH) == 0) {
        memcpy(blockchain_data.blockchain_reserve_bytes.block_producer_delegates_name,
               current_block_verifiers_list.block_verifiers_name[count],
               strnlen(current_block_verifiers_list.block_verifiers_name[count],
                       sizeof(current_block_verifiers_list.block_verifiers_name[count])));
        memcpy(blockchain_data.blockchain_reserve_bytes.block_producer_public_address,
               current_block_verifiers_list.block_verifiers_public_address[count],
               XCASH_WALLET_LENGTH);
        break;
      }
    }
  }

  // Map backup node indexes
  for (size_t backup_idx = 0; backup_idx < 5; backup_idx++) {
    for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strncmp(backup_public_addresses[backup_idx + 1], current_block_verifiers_list.block_verifiers_public_address[count], XCASH_WALLET_LENGTH) == 0) {
        block_producer_backup_settings[backup_idx] = (int)count;
        break;
      }
    }
  }

  // Backup node index in blockchain data
  memcpy(blockchain_data.blockchain_reserve_bytes.block_producer_node_backup_count,
         current_round_part_backup_node, sizeof(char));

  // Add backup node names, comma-separated
  blockchain_data.blockchain_reserve_bytes.block_producer_backup_nodes_names[0] = '\0';  // Init empty string
  for (size_t i = 0; i < 5; i++) {
    size_t len = strnlen(current_block_verifiers_list.block_verifiers_name[block_producer_backup_settings[i]],
                         sizeof(current_block_verifiers_list.block_verifiers_name[block_producer_backup_settings[i]]));
    strcat(blockchain_data.blockchain_reserve_bytes.block_producer_backup_nodes_names,
           current_block_verifiers_list.block_verifiers_name[block_producer_backup_settings[i]]);
    if (i != 4) {
      strcat(blockchain_data.blockchain_reserve_bytes.block_producer_backup_nodes_names, ",");
    }
  }

  // Add VRF data
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_secret_key, VRF_data.vrf_secret_key, crypto_vrf_SECRETKEYBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_secret_key_data, VRF_data.vrf_secret_key_data, VRF_SECRET_KEY_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_public_key, VRF_data.vrf_public_key, crypto_vrf_PUBLICKEYBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_public_key_data, VRF_data.vrf_public_key_data, VRF_PUBLIC_KEY_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_alpha_string, VRF_data.vrf_alpha_string, strnlen((const char*)VRF_data.vrf_alpha_string, BUFFER_SIZE));
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_alpha_string_data, VRF_data.vrf_alpha_string_data, strnlen(VRF_data.vrf_alpha_string_data, BUFFER_SIZE));
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_proof, VRF_data.vrf_proof, crypto_vrf_PROOFBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_proof_data, VRF_data.vrf_proof_data, VRF_PROOF_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_beta_string, VRF_data.vrf_beta_string, crypto_vrf_OUTPUTBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_beta_string_data, VRF_data.vrf_beta_string_data, VRF_BETA_LENGTH);

  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_secret_key[count], VRF_data.block_verifiers_vrf_secret_key[count], crypto_vrf_SECRETKEYBYTES);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_public_key[count], VRF_data.block_verifiers_vrf_public_key[count], crypto_vrf_PUBLICKEYBYTES);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_secret_key_data[count], VRF_data.block_verifiers_vrf_secret_key_data[count], VRF_SECRET_KEY_LENGTH);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_public_key_data[count], VRF_data.block_verifiers_vrf_public_key_data[count], VRF_PUBLIC_KEY_LENGTH);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_random_data_text[count], VRF_data.block_verifiers_random_data[count], RANDOM_STRING_LENGTH);

    memcpy(blockchain_data.blockchain_reserve_bytes.next_block_verifiers_public_address[count], next_block_verifiers_list.block_verifiers_public_key[count], VRF_PUBLIC_KEY_LENGTH);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_validation_node_signature_data[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE_DATA, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE_DATA) - 1);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_validation_node_signature[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE) - 1);

    for (counter = 0, count2 = 0; counter < RANDOM_STRING_LENGTH; counter++, count2 += 2) {
      snprintf(blockchain_data.blockchain_reserve_bytes.block_verifiers_random_data[count] + count2, RANDOM_STRING_LENGTH, "%02x", VRF_data.block_verifiers_random_data[count][counter] & 0xFF);
    }
  }

  memcpy(blockchain_data.blockchain_reserve_bytes.previous_block_hash_data, blockchain_data.previous_block_hash_data, BLOCK_HASH_LENGTH);

  // Convert blockchain_data to network block string
  if (blockchain_data_to_network_block_string(VRF_data.block_blob, BLOCK_VERIFIERS_AMOUNT) == 0) {
    ERROR_PRINT("Could not convert the blockchain_data to a network_block_string");
    return XCASH_ERROR;
  }

  // Sign network block string
  memset(data, 0, sizeof(data));
  if (sign_network_block_string(data, VRF_data.block_blob) == 0) {
    ERROR_PRINT("Could not sign the network block string");
    return XCASH_ERROR;
  }

  // Add signature to VRF data
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count], xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
      memcpy(VRF_data.block_blob_signature[count], data, strnlen(data, BUFFER_SIZE));
      break;
    }
  }

  // Construct message
  // create the message
  memset(message,0,strlen(message));
  memcpy(message,"{\r\n \"message_settings\": \"BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE\",\r\n \"block_blob_signature\": \"",110);
  memcpy(message+110,data,strnlen(data,BUFFER_SIZE));
  memcpy(message+strlen(message),"\",\r\n}",5);
  return XCASH_OK;
}

// Helper function: Sync fallback logic
int sync_with_backup_fallback(int backup_index, int primary_min, int primary_sec, int backup_min, int backup_sec) {
  if (backup_index == 0) {
      return sync_block_verifiers_minutes_and_seconds(primary_min, primary_sec);
  } else {
      return sync_block_verifiers_minutes_and_seconds(backup_min, backup_sec);
  }
}

// Helper function: Check majority and log
int check_majority(size_t valid_count, const char* stage_description) {
  if (valid_count >= BLOCK_VERIFIERS_VALID_AMOUNT) {
      INFO_PRINT_STATUS_OK("[%zu / %d] block verifiers have a majority for %s", valid_count, BLOCK_VERIFIERS_VALID_AMOUNT, stage_description);
      return XCASH_OK;
  } else {
      INFO_PRINT_STATUS_FAIL("[%zu / %d] block verifiers lack majority for %s", valid_count, BLOCK_VERIFIERS_VALID_AMOUNT, stage_description);
      WARNING_PRINT("Insufficient majority for %s", stage_description);
      return XCASH_ERROR;
  }
}

// Helper function: Restart logic if alone verifier
int check_restart_if_alone(size_t count) {
  if (count <= 1) {
      for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
          if (strncmp(current_block_verifiers_list.block_verifiers_public_address[i], xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
              WARNING_PRINT("Restarting, could not process any other block verifiers data");
              return 1;
          }
      }
  }
  return 0;
}

// Helper function: Send data & cleanup
int send_and_cleanup(const char* data) {
  response_t** replies = NULL;
  if (!xnet_send_data_multi(XNET_DELEGATES_ALL_ONLINE, data, &replies)) {
      cleanup_responses(replies);
      return XCASH_ERROR;
  }
  cleanup_responses(replies);
  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_block
Description: Runs the round where the block verifiers will create the block
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int block_verifiers_create_block(size_t round_number) {
  char data[BUFFER_SIZE] = {0};
  char data2[BUFFER_SIZE] = {0};
  size_t count, count2;
  int backup_node_index = current_round_part_backup_node[0] = (char)('0' + round_number) - '0';

  // Clear all VRF data
  memset(&VRF_data, 0, sizeof(VRF_data));
  memset(&current_block_verifiers_majority_vote, 0, sizeof(current_block_verifiers_majority_vote));

  // Initial sync
  INFO_STAGE_PRINT("Waiting for block synchronization start time...");
  if (sync_with_backup_fallback(backup_node_index, 0, 30, 3, 5) == XCASH_ERROR)
      return ROUND_SKIP;

  // Check block height
  if (get_current_block_height(data) == 1 && strncmp(current_block_height, data, BUFFER_SIZE) != 0) {
      WARNING_PRINT("Your block height is not synced correctly, waiting for next round");
      replayed_round_settings = 1;
      return ROUND_NEXT;
  }

  // Get previous block hash
  if (get_previous_block_hash(previous_block_hash) == 0) {
      WARNING_PRINT("Could not get previous block hash");
      return ROUND_NEXT;
  }

  // Part 0 - Sync block producers
  INFO_STAGE_PRINT("Part 0 - Exchanging block producers list");
  if (!sync_block_producers()) {
      WARNING_PRINT("Can't select block producer");
      return ROUND_NEXT;
  }

  // Part 1 - Create VRF data
  INFO_STAGE_PRINT("Part 1 - Create VRF data");
  if (block_verifiers_create_VRF_secret_key_and_VRF_public_key(data) == 0 || sign_data(data) == 0) {
      WARNING_PRINT("Could not create VRF data");
      return ROUND_NEXT;
  }
  INFO_PRINT_STATUS_OK("The VRF data has been created");

  // Part 2 - Send VRF data
  INFO_STAGE_PRINT("Part 2 - Send VRF data to all block verifiers");
  if (!send_and_cleanup(data)) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("The VRF data has been sent");

  // Part 3 - Wait for VRF sync
  if (sync_with_backup_fallback(backup_node_index, 1, 10, 3, 15) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 4 - Create individual majority VRF data
  INFO_STAGE_PRINT("Part 4 - Create each individual majority VRF data");
  memset(data, 0, sizeof(data));
  block_verifiers_create_vote_majority_results(data, 0);
  if (sign_data(data) == 0) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Each individual majority VRF data created");

  // Part 5 - Send individual VRF majority
  INFO_STAGE_PRINT("Part 5 - Send individual majority VRF data");
  if (!send_and_cleanup(data)) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Individual majority VRF data sent");

  // Part 6 - Wait
  if (sync_with_backup_fallback(backup_node_index, 1, 25, 3, 25) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 7 - Check VRF majority (temp fix applied)
  INFO_STAGE_PRINT("Part 7 - Check VRF majority");
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strlen(VRF_data.block_verifiers_vrf_secret_key_data[count]) == 0) {
          memcpy(VRF_data.block_verifiers_vrf_secret_key_data[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA) - 1);
          memcpy(VRF_data.block_verifiers_vrf_secret_key[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY) - 1);
          memcpy(VRF_data.block_verifiers_vrf_public_key_data[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA) - 1);
          memcpy(VRF_data.block_verifiers_vrf_public_key[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY) - 1);
          memcpy(VRF_data.block_verifiers_random_data[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING) - 1);
      }
  }
  count = BLOCK_VERIFIERS_AMOUNT;
  INFO_PRINT_STATUS_OK("Checked VRF data majority");

  // Part 8 - Majority check
  if (!check_majority(count, "VRF data")) return ROUND_NEXT;
  if (check_restart_if_alone(count)) return ROUND_SKIP;

  // Part 9 - Check overall majority
  INFO_STAGE_PRINT("Part 9 - Check overall majority for VRF data");
  memset(delegates_error_list,0,sizeof(delegates_error_list)); \
  memcpy(delegates_error_list,"The following delegates are reported as not working for this part:",66);
  count2 = 0;
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strncmp(VRF_data.block_verifiers_vrf_secret_key_data[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA, BUFFER_SIZE) != 0)
          count2++;
      else {
          strcat(delegates_error_list, current_block_verifiers_list.block_verifiers_name[count]);
          strcat(delegates_error_list, "|");
      }
  }
  if (!check_majority(count2, "overall VRF data")) return ROUND_NEXT;
  if (check_restart_if_alone(count2)) return ROUND_SKIP;

  // Part 10 - Select VRF data
  INFO_STAGE_PRINT("Part 10 - Select VRF data to use");
  if (block_verifiers_create_VRF_data() == 0) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("VRF data selected");

  // Part 11 - Create or wait for block template
  if (strcmp(producer_refs[round_number].public_address, xcash_wallet_public_address) == 0) {
      INFO_STAGE_PRINT("Part 11 - Create and send block template");
      if (get_block_template(VRF_data.block_blob) == 0) return ROUND_NEXT;
      memset(data, 0, sizeof(data));
      snprintf(data, sizeof(data), "{\r\n \"message_settings\": \"MAIN_NODES_TO_NODES_PART_4_OF_ROUND_CREATE_NEW_BLOCK\",\r\n \"block_blob\": \"%s\",\r\n}", VRF_data.block_blob);
      if (sign_data(data) == 0) return ROUND_NEXT;
      if (!send_and_cleanup(data)) return ROUND_NEXT;
  } else {
      INFO_STAGE_PRINT("Part 11 - Wait for block template");
  }

  if (sync_with_backup_fallback(backup_node_index, 2, 20, 3, 35) == XCASH_ERROR)
      return ROUND_SKIP;

  if (strncmp(VRF_data.block_blob, "", 1) == 0) {
      WARNING_PRINT("Did not receive block template");
      return ROUND_NEXT;
  }
  INFO_PRINT_STATUS_OK("Received block template");

  // Part 12 - Add VRF data and sign block
  INFO_STAGE_PRINT("Part 12 - Add VRF data and sign block template");
  if (block_verifiers_create_block_signature(data) == 0 || sign_data(data) == 0)
      return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Block template signed");

  // Part 13 - Send block signature
  INFO_STAGE_PRINT("Part 13 - Send block template signature");
  if (!send_and_cleanup(data)) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Sent block template signature");

  if (sync_with_backup_fallback(backup_node_index, 2, 30, 3, 45) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 15 - Create individual majority block signatures
  INFO_STAGE_PRINT("Part 15 - Create majority block template signature");
  memset(data, 0, sizeof(data));
  block_verifiers_create_vote_majority_results(data, 1);
  if (sign_data(data) == 0) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Created majority block template signature");

  // Part 16 - Send majority block signatures
  INFO_STAGE_PRINT("Part 16 - Send majority block template signature");
  if (!send_and_cleanup(data)) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Sent majority block template signature");

  if (sync_with_backup_fallback(backup_node_index, 2, 40, 3, 55) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 18 - Check individual majority signatures
  INFO_STAGE_PRINT("Part 18 - Check block template signature majority");
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strlen(VRF_data.block_blob_signature[count]) == 0)
          memcpy(VRF_data.block_blob_signature[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE) - 1);
  }
  count = BLOCK_VERIFIERS_AMOUNT;
  INFO_PRINT_STATUS_OK("Checked block template signature majority");

  if (!check_majority(count, "block template signature")) return ROUND_NEXT;
  if (check_restart_if_alone(count)) return ROUND_SKIP;

  // Part 20 - Overall majority signature
  INFO_STAGE_PRINT("Part 20 - Check overall block template signature majority");
  RESET_DELEGATE_ERROR_MESSAGE;
  count2 = 0;
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strncmp(VRF_data.block_blob_signature[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE, BUFFER_SIZE) != 0)
          count2++;
      else {
          strcat(delegates_error_list, current_block_verifiers_list.block_verifiers_name[count]);
          strcat(delegates_error_list, "|");
      }
  }
  if (!check_majority(count2, "overall block template signature")) return ROUND_NEXT;
  if (check_restart_if_alone(count2)) return ROUND_SKIP;

  // Part 22 - Create vote results for reserve bytes
  if (block_verifiers_create_vote_results(data) == 0 || sign_data(data) == 0) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Created reserve bytes majority data");

  if (sync_with_backup_fallback(backup_node_index, 2, 45, BLOCK_TIME - 1, 0) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 23 - Send reserve bytes data
  INFO_STAGE_PRINT("Part 23 - Send reserve bytes data");
  if (!send_and_cleanup(data)) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Sent reserve bytes data");

  if (sync_with_backup_fallback(backup_node_index, 2, 55, BLOCK_TIME - 1, 10) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 25 - Final reserve bytes majority check
  INFO_STAGE_PRINT("Part 25 - Check reserve bytes majority");
  if (current_round_part_vote_data.vote_results_valid >= BLOCK_VERIFIERS_VALID_AMOUNT) {
      INFO_PRINT_STATUS_OK("[%d / %d] reserve bytes majority", current_round_part_vote_data.vote_results_valid, BLOCK_VERIFIERS_VALID_AMOUNT);
  } else {
      WARNING_PRINT("Invalid reserve bytes majority");
      return ROUND_NEXT;
  }

  // Final step - Update database
  if (block_verifiers_create_block_and_update_database() == XCASH_ERROR)
      return ROUND_ERROR;

  return ROUND_OK;
}