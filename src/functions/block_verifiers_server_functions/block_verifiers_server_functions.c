#include "block_verifiers_server_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data
Description: Runs the code when the server receives the BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA message
Parameters:
  MESSAGE - The message
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(const char* MESSAGE)
{
  char public_address[XCASH_WALLET_LENGTH+1] = {0};
  char vrf_secret_key_data[VRF_SECRET_KEY_LENGTH+1] = {0};
  char vrf_public_key_data[VRF_PUBLIC_KEY_LENGTH+1] = {0};
  char random_data[RANDOM_STRING_LENGTH+1] = {0};
  char data[MAXIMUM_NUMBER_SIZE] = {0};
  char vrf_proof[VRF_PROOF_LENGTH + 1] = {0};
  char vrf_beta[VRF_BETA_LENGTH + 1] = {0};
  unsigned char vrf_public_key[crypto_vrf_PUBLICKEYBYTES];
  unsigned char vrf_secret_key[crypto_vrf_SECRETKEYBYTES];

  int count;
  int counter;

  DEBUG_PRINT("received %s, %s", __func__, MESSAGE);

  memset(public_address,0,sizeof(public_address));
  memset(vrf_secret_key_data,0,sizeof(vrf_secret_key_data));
  memset(vrf_public_key_data,0,sizeof(vrf_public_key_data));
  memset(random_data,0,sizeof(random_data));
  memset(data,0,sizeof(data));  
  memset(vrf_public_key,0,sizeof(vrf_public_key));
  memset(vrf_secret_key,0,sizeof(vrf_secret_key));

  // parse the message
  if (
    parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == 0 ||
    strlen(public_address) != XCASH_WALLET_LENGTH ||
    strncmp(public_address, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||  
    parse_json_data(MESSAGE, "vrf_secret_key", vrf_secret_key_data, sizeof(vrf_secret_key_data)) == 0 ||
    strlen(vrf_secret_key_data) != VRF_SECRET_KEY_LENGTH ||
    parse_json_data(MESSAGE, "vrf_public_key", vrf_public_key_data, sizeof(vrf_public_key_data)) == 0 ||
    strlen(vrf_public_key_data) != VRF_PUBLIC_KEY_LENGTH ||
    parse_json_data(MESSAGE, "random_data", random_data, sizeof(random_data)) == 0 ||
    strlen(random_data) != RANDOM_STRING_LENGTH ||
    parse_json_data(MESSAGE, "vrf_proof", vrf_proof, sizeof(vrf_proof)) == 0 ||
    strlen(vrf_proof) != VRF_PROOF_LENGTH ||
    parse_json_data(MESSAGE, "vrf_beta", vrf_beta, sizeof(vrf_beta)) == 0 ||
    strlen(vrf_beta) != VRF_BETA_LENGTH
  )
  {
    ERROR_PRINT("Could not parse the block_verifiers_to_block_verifiers_vrf_data");
    return;
  }

  // convert the VRF secret key string to a VRF secret key
  for (counter = 0, count = 0; counter < VRF_SECRET_KEY_LENGTH; count++, counter += 2)
  {
    memset(data,0,strlen(data));
    memcpy(data,&vrf_secret_key_data[counter],2);
    vrf_secret_key[count] = (unsigned char)strtol(data, NULL, 16);
  } 

  // convert the VRF public key string to a VRF public key
  for (counter = 0, count = 0; counter < VRF_PUBLIC_KEY_LENGTH; count++, counter += 2)
  {
    memset(data,0,strlen(data));
    memcpy(data,&vrf_public_key_data[counter],2);
    vrf_public_key[count] = (unsigned char)strtol(data, NULL, 16);
  } 

  // process the vote data
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count], public_address,
                  XCASH_WALLET_LENGTH) == 0 &&
          strncmp(VRF_data.block_verifiers_vrf_secret_key_data[count], "", 1) == 0 &&
          strncmp((char*)VRF_data.block_verifiers_vrf_secret_key[count], "", 1) == 0 &&
          strncmp(VRF_data.block_verifiers_vrf_public_key_data[count], "", 1) == 0 &&
          strncmp((char*)VRF_data.block_verifiers_vrf_public_key[count], "", 1) == 0 &&
          strncmp(VRF_data.block_verifiers_random_data[count], "", 1) == 0) {
          memcpy(VRF_data.block_verifiers_vrf_secret_key_data[count], vrf_secret_key_data, VRF_SECRET_KEY_LENGTH);
          memcpy(VRF_data.block_verifiers_vrf_secret_key[count], vrf_secret_key, crypto_vrf_SECRETKEYBYTES);
          memcpy(VRF_data.block_verifiers_vrf_public_key_data[count], vrf_public_key_data, VRF_PUBLIC_KEY_LENGTH);
          memcpy(VRF_data.block_verifiers_vrf_public_key[count], vrf_public_key, crypto_vrf_PUBLICKEYBYTES);
          memcpy(VRF_data.block_verifiers_random_data[count], random_data, RANDOM_STRING_LENGTH);
      }
  }

  return;
}






void server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(const char* MESSAGE)
{
  char public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char vrf_secret_key_data[VRF_SECRET_KEY_LENGTH + 1] = {0};
  char vrf_public_key_data[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  char random_data[RANDOM_STRING_LENGTH + 1] = {0};
  char vrf_proof[VRF_PROOF_LENGTH + 1] = {0};
  char vrf_beta[VRF_BETA_LENGTH + 1] = {0};
  char byte_str[3] = {0};
  unsigned char vrf_public_key[crypto_vrf_PUBLICKEYBYTES] = {0};
  unsigned char vrf_secret_key[crypto_vrf_SECRETKEYBYTES] = {0};

  DEBUG_PRINT("Received %s, %s", __func__, MESSAGE);

  // Parse and validate input
  if (
    parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == 0 ||
    strlen(public_address) != XCASH_WALLET_LENGTH ||
    strncmp(public_address, XCASH_WALLET_PREFIX, strlen(XCASH_WALLET_PREFIX)) != 0 ||

    parse_json_data(MESSAGE, "vrf_secret_key", vrf_secret_key_data, sizeof(vrf_secret_key_data)) == 0 ||
    strlen(vrf_secret_key_data) != VRF_SECRET_KEY_LENGTH ||

    parse_json_data(MESSAGE, "vrf_public_key", vrf_public_key_data, sizeof(vrf_public_key_data)) == 0 ||
    strlen(vrf_public_key_data) != VRF_PUBLIC_KEY_LENGTH ||

    parse_json_data(MESSAGE, "random_data", random_data, sizeof(random_data)) == 0 ||
    strlen(random_data) != RANDOM_STRING_LENGTH ||

    parse_json_data(MESSAGE, "vrf_proof", vrf_proof, sizeof(vrf_proof)) == 0 ||
    strlen(vrf_proof) != VRF_PROOF_LENGTH ||

    parse_json_data(MESSAGE, "vrf_beta", vrf_beta, sizeof(vrf_beta)) == 0 ||
    strlen(vrf_beta) != VRF_BETA_LENGTH
  ) {
    ERROR_PRINT("Could not parse the BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA message");
    return;
  }

  // Convert hex secret key to binary
  for (int i = 0; i < crypto_vrf_SECRETKEYBYTES; i++) {
    memcpy(byte_str, &vrf_secret_key_data[i * 2], 2);
    vrf_secret_key[i] = (unsigned char)strtol(byte_str, NULL, 16);
  }

  // Convert hex public key to binary
  for (int i = 0; i < crypto_vrf_PUBLICKEYBYTES; i++) {
    memcpy(byte_str, &vrf_public_key_data[i * 2], 2);
    vrf_public_key[i] = (unsigned char)strtol(byte_str, NULL, 16);
  }

  // Store values in global VRF_data for this verifier
  for (int i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[i], public_address, XCASH_WALLET_LENGTH) == 0 &&
        strncmp(VRF_data.block_verifiers_vrf_secret_key_data[i], "", 1) == 0) {
      
      memcpy(VRF_data.block_verifiers_vrf_secret_key_data[i], vrf_secret_key_data, VRF_SECRET_KEY_LENGTH);
      memcpy(VRF_data.block_verifiers_vrf_secret_key[i], vrf_secret_key, crypto_vrf_SECRETKEYBYTES);
      memcpy(VRF_data.block_verifiers_vrf_public_key_data[i], vrf_public_key_data, VRF_PUBLIC_KEY_LENGTH);
      memcpy(VRF_data.block_verifiers_vrf_public_key[i], vrf_public_key, crypto_vrf_PUBLICKEYBYTES);
      memcpy(VRF_data.block_verifiers_random_data[i], random_data, RANDOM_STRING_LENGTH);
      memcpy(VRF_data.block_verifiers_vrf_proof_data[i], vrf_proof, VRF_PROOF_LENGTH);
      memcpy(VRF_data.block_verifiers_vrf_beta_data[i], vrf_beta, VRF_BETA_LENGTH);
      break;
    }
  }
}