#include "block_verifiers_server_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data
Description: Runs the code when the server receives the BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA message
Parameters:
  MESSAGE - The message
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(const char* MESSAGE)
{
  char public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char vrf_public_key_data[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  char vrf_proof_hex[VRF_PROOF_LENGTH + 1] = {0};  
  char vrf_beta_hex[VRF_BETA_LENGTH + 1] = {0};
  char random_buf_hex[(VRF_RANDOMBYTES_LENGTH * 2) + 1] = {0};
  char block_height[BLOCK_HEIGHT_LENGTH] = {0};
  int count;

  DEBUG_PRINT("received %s, %s", __func__, MESSAGE);


  int wait_seconds = 0;
  while (atomic_load(&wait_for_vrf_init) && wait_seconds < DELAY_EARLY_TRANSACTIONS_MAX) {
    sleep(1);
    wait_seconds++;
  }
  if (atomic_load(&wait_for_vrf_init)) {
    ERROR_PRINT("Timed out waiting for vrf init in server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data");
  }

  // parse the message
  if (parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == XCASH_ERROR || 
    parse_json_data(MESSAGE, "vrf_public_key", vrf_public_key_data, sizeof(vrf_public_key_data)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "random_data", random_buf_hex, sizeof(random_buf_hex)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "vrf_proof", vrf_proof_hex, sizeof(vrf_proof_hex)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "vrf_beta", vrf_beta_hex, sizeof(vrf_beta_hex)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "block-height", block_height, sizeof(block_height)) == XCASH_ERROR)
  {
    ERROR_PRINT("Could not parse the block_verifiers_to_block_verifiers_vrf_data");
    return;
  }

  if (strcmp(block_height, current_block_height) != 0) {
      INFO_PRINT("Skipping VRF data: current block_height [%s] does not match expected [%s]", current_block_height, block_height);
      return; 
  }

  pthread_mutex_lock(&majority_vrf_lock);
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count], public_address, XCASH_WALLET_LENGTH) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_random_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_proof_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_beta_hex[count], "", 1) == 0) {

      unsigned char random_buf_bin[VRF_RANDOMBYTES_LENGTH] = {0};
      unsigned char alpha_input_bin[VRF_RANDOMBYTES_LENGTH * 2] = {0};
      unsigned char pk_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
      unsigned char vrf_proof[crypto_vrf_PROOFBYTES] = {0};
      unsigned char vrf_beta[crypto_vrf_OUTPUTBYTES] = {0};
      unsigned char previous_block_hash_bin[BLOCK_HASH_LENGTH / 2] = {0};

      if (!hex_to_byte_array(vrf_public_key_data, pk_bin, sizeof(pk_bin)) ||

        !hex_to_byte_array(vrf_proof_hex, vrf_proof, sizeof(vrf_proof)) ||
        !hex_to_byte_array(vrf_beta_hex, vrf_beta, sizeof(vrf_beta)) ||
        !hex_to_byte_array(random_buf_hex, random_buf_bin, sizeof(random_buf_bin)) ||
        !hex_to_byte_array(previous_block_hash, previous_block_hash_bin, sizeof(previous_block_hash_bin))) {

          ERROR_PRINT("Failed to decode one or more fields in VRF message from %s", public_address);
          break;
      }

      // Form alpha input = previous_block_hash || random_buf
      memcpy(alpha_input_bin, previous_block_hash_bin, VRF_RANDOMBYTES_LENGTH);
      memcpy(alpha_input_bin + VRF_RANDOMBYTES_LENGTH, random_buf_bin, VRF_RANDOMBYTES_LENGTH);

      // Verify VRF proof
      unsigned char computed_beta[crypto_vrf_OUTPUTBYTES];
      if (crypto_vrf_verify(computed_beta, pk_bin, vrf_proof, alpha_input_bin, sizeof(alpha_input_bin)) != 0) {
        ERROR_PRINT("VRF proof failed verification from %s", public_address);
        break;
      }

      if (memcmp(computed_beta, vrf_beta, sizeof(vrf_beta)) != 0) {
        WARNING_PRINT("VRF beta mismatch from %s", public_address);
        break;
      }

      memcpy(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[count], vrf_public_key_data, VRF_PUBLIC_KEY_LENGTH+1);
      memcpy(current_block_verifiers_list.block_verifiers_random_hex[count], random_buf_hex, VRF_RANDOMBYTES_LENGTH * 2 + 1);
      memcpy(current_block_verifiers_list.block_verifiers_vrf_proof_hex[count], vrf_proof_hex, VRF_PROOF_LENGTH + 1); 
      memcpy(current_block_verifiers_list.block_verifiers_vrf_beta_hex[count], vrf_beta_hex, VRF_BETA_LENGTH + 1);

      break;
    }
  }
  pthread_mutex_unlock(&majority_vrf_lock);

  return;
}

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_node_to_node_majority
Description: Runs the code when the server receives the NODES_TO_NODES_VOTE_MAJORITY_RESULTS message
Parameters:
  MESSAGE - The message
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_node_to_node_majority(const char* MESSAGE)
{
    // Variables
    char key_buffer[32]; // Sufficient for "vote_data_" + max index
    char public_address[XCASH_WALLET_LENGTH + 1] = {0};
    int sender_index = -1;

    DEBUG_PRINT("Received %s, %s", __func__, MESSAGE);

    // Parse public address
    if (parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == 0) {
        ERROR_PRINT("Can't parse public_address");
        return;
    }

    // Find the verifier index
    for (int i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
        if (strncmp(current_block_verifiers_list.block_verifiers_public_address[i], public_address, XCASH_WALLET_LENGTH) == 0) {
            sender_index = i;
            break;
        }
    }

    if (sender_index == -1) {
        ERROR_PRINT("Unknown verifier address: %s", public_address);
        return;
    }

    pthread_mutex_lock(&majority_vote_lock);








/*
    for (int receiver_index = 0; receiver_index < BLOCK_VERIFIERS_AMOUNT; receiver_index++) {
        snprintf(key_buffer, sizeof(key_buffer), "vote_data_%d", receiver_index + 1);

        if (parse_json_data(MESSAGE, key_buffer,
                            current_block_verifiers_majority_vote.data[sender_index][receiver_index],
                            sizeof(current_block_verifiers_majority_vote.data[sender_index][receiver_index])) == 0) {
            ERROR_PRINT("Could not parse vote_data_%d from %s", receiver_index + 1, public_address);
            pthread_mutex_unlock(&majority_vote_lock);
            return;
        }
    }
*/
    pthread_mutex_unlock(&majority_vote_lock);
    return;
}
