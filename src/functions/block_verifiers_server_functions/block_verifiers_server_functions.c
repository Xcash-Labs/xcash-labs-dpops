#include "block_verifiers_server_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data
Description: Runs the code when the server receives the BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA message
Parameters:
  MESSAGE - The message
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(const char* MESSAGE)
{
  unsigned char vrf_pk_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
  char public_address[XCASH_WALLET_LENGTH+1] = {0};
  char random_data[RANDOM_STRING_LENGTH+1] = {0};
  char data[MAXIMUM_NUMBER_SIZE] = {0};
  char vrf_proof[VRF_PROOF_LENGTH + 1] = {0};
  char vrf_beta[VRF_BETA_LENGTH + 1] = {0};
  char vrf_public_key_data[VRF_PUBLIC_KEY_LENGTH+1] = {0};
  char block_part[BLOCK_HEIGHT_LENGTH] = {0};
  char expected_block_part[BLOCK_HEIGHT_LENGTH] = {0};
  int counter;

  DEBUG_PRINT("received %s, %s", __func__, MESSAGE);

  // parse the message
  if (
    parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == 0 ||
    strlen(public_address) != XCASH_WALLET_LENGTH ||
    strncmp(public_address, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||  
    parse_json_data(MESSAGE, "vrf_public_key", vrf_public_key_data, sizeof(vrf_public_key_data)) == 0 ||
    strlen(vrf_public_key_data) != VRF_PUBLIC_KEY_LENGTH ||
    parse_json_data(MESSAGE, "random_data", random_data, sizeof(random_data)) == 0 ||
    strlen(random_data) != RANDOM_STRING_LENGTH ||
    parse_json_data(MESSAGE, "vrf_proof", vrf_proof, sizeof(vrf_proof)) == 0 ||
    strlen(vrf_proof) != VRF_PROOF_LENGTH ||
    parse_json_data(MESSAGE, "vrf_beta", vrf_beta, sizeof(vrf_beta)) == 0 ||
    strlen(vrf_beta) != VRF_BETA_LENGTH ||
    parse_json_data(MESSAGE, "block-part", block_part, sizeof(block_part)) == 0 ||
    strlen(block_part) < 3 // basic sanity check
  )
  {
    ERROR_PRINT("Could not parse the block_verifiers_to_block_verifiers_vrf_data");
    return;
  }

  snprintf(expected_block_part, sizeof(expected_block_part), "%s-P1", current_block_height);
    if (strcmp(block_part, expected_block_part) != 0) {
      INFO_PRINT("Skipping VRF data: block_part [%s] does not match expected [%s]", block_part, expected_block_part);
      return;
  }
  
  if (hex_to_byte_array(vrf_public_key_data, vrf_pk_bin, crypto_vrf_PUBLICKEYBYTES) != XCASH_OK) {
    ERROR_PRINT("Failed to parse vrf_public_key_data into bytes.");
    return;
  }

  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count], public_address, XCASH_WALLET_LENGTH) == 0 &&
        strncmp(VRF_data.block_verifiers_vrf_public_key_data[count], "", 1) == 0 &&
        strncmp((char*)VRF_data.block_verifiers_vrf_public_key[count], "", 1) == 0 &&
        strncmp(VRF_data.block_verifiers_random_data[count], "", 1) == 0 &&
        strncmp(VRF_data.block_verifiers_vrf_proof_data[count], "", 1) == 0 &&
        strncmp(VRF_data.block_verifiers_vrf_beta_data[count], "", 1) == 0) {
      memcpy(VRF_data.block_verifiers_vrf_public_key_data[count], vrf_public_key_data, VRF_PUBLIC_KEY_LENGTH + 1);
      memcpy(VRF_data.block_verifiers_vrf_public_key[count], vrf_pk_bin, crypto_vrf_PUBLICKEYBYTES);
      memcpy(VRF_data.block_verifiers_random_data[count], random_data, RANDOM_STRING_LENGTH + 1);
      memcpy(VRF_data.block_verifiers_vrf_proof_data[count], vrf_proof, VRF_PROOF_LENGTH + 1);
      memcpy(VRF_data.block_verifiers_vrf_beta_data[count], vrf_beta, VRF_BETA_LENGTH + 1);
      break;
    }
  }

  return;
}