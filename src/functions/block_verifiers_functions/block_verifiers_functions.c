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
  snprintf(message, BUFFER_SIZE,
           "{\r\n \"message_settings\": \"BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE\",\r\n \"block_blob_signature\": \"%s\",\r\n}",
           data);

  return XCASH_OK;
}