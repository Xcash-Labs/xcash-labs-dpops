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

  // parse the message
  if (
    parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == 0 ||
    strlen(public_address) != XCASH_WALLET_LENGTH ||
    strncmp(public_address, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||  
    parse_json_data(MESSAGE, "vrf_public_key", vrf_public_key_data, sizeof(vrf_public_key_data)) == 0 ||
    strlen(vrf_public_key_data) != VRF_PUBLIC_KEY_LENGTH ||
    parse_json_data(MESSAGE, "random_data", random_buf_hex, sizeof(random_buf_hex)) == 0 ||
    strlen(random_buf_hex) != RANDOM_STRING_LENGTH ||
    parse_json_data(MESSAGE, "vrf_proof", vrf_proof_hex, sizeof(vrf_proof_hex)) == 0 ||
    strlen(vrf_proof_hex) != VRF_PROOF_LENGTH ||
    parse_json_data(MESSAGE, "vrf_beta", vrf_beta_hex, sizeof(vrf_beta_hex)) == 0 ||
    strlen(vrf_beta_hex) != VRF_BETA_LENGTH ||
    parse_json_data(MESSAGE, "block-height", block_height, sizeof(block_height)) == 0 ||
    strlen(block_part) < 3 // basic sanity check
  )
  {
    ERROR_PRINT("Could not parse the block_verifiers_to_block_verifiers_vrf_data");
    return;
  }

  if (strcmp(block_height, current_block_height) != 0) {
      INFO_PRINT("Skipping VRF data: block_part [%s] does not match expected [%s]", block_part, expected_block_part);
      return;
  }
  
  pthread_mutex_lock(&majority_vote_lock);
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count], public_address, XCASH_WALLET_LENGTH) == 0 &&
    strncmp(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_random_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_proof_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_beta_hex[count], "", 1) == 0) {
          memcpy(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[count], vrf_public_key, VRF_PUBLIC_KEY_LENGTH+1);
          memcpy(current_block_verifiers_list.block_verifiers_random_hex[count], random_buf_hex, VRF_RANDOMBYTES_LENGTH * 2 + 1);
          memcpy(current_block_verifiers_list.block_verifiers_vrf_proof_hex[count], vrf_proof_hex, VRF_PROOF_LENGTH + 1); 
          memcpy(current_block_verifiers_list.block_verifiers_vrf_beta_hex[count], vrf_beta_hex, VRF_BETA_LENGTH + 1);
      break;
    }
  }
  pthread_mutex_unlock(&majority_vote_lock);

  return;
}