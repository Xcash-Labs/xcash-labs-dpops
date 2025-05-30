#include "VRF_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: create_random_VRF_keys
Description: Creates a random seed, and uses the seed to generate random VRF public key and a random VRF secret key
Parameters:
  public_key - The VRF public key
  secret_key - The VRF secret key
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int create_random_VRF_keys(unsigned char *VRF_public_key, unsigned char *VRF_secret_key)
{
  // Variables
  unsigned char data[crypto_vrf_SEEDBYTES + 1];
  if (getrandom(data, crypto_vrf_SEEDBYTES, 0) != crypto_vrf_SEEDBYTES)
  {
    return 0;
  }
  // create the VRF private and secret key
  crypto_vrf_keypair_from_seed(VRF_public_key, VRF_secret_key, data);
  return 1;
}

/*---------------------------------------------------------------------------------------------------------
Name: generate_key
Description: Generates a random public and private key, to be used for the signing and verifying of messages between the block verifiers
---------------------------------------------------------------------------------------------------------*/
void generate_key()
{
  // Variables
  unsigned char vrf_secret_key_data[crypto_vrf_SECRETKEYBYTES];
  unsigned char vrf_public_key_data[crypto_vrf_PUBLICKEYBYTES];
  char vrf_secret_key_hex[(crypto_vrf_SECRETKEYBYTES * 2) + 1];
  char vrf_public_key_hex[(crypto_vrf_PUBLICKEYBYTES * 2) + 1];
  
  int count;
  int count2;
  memset(vrf_secret_key_hex, 0, sizeof(vrf_secret_key_hex));
  memset(vrf_public_key_hex, 0, sizeof(vrf_public_key_hex));
  memset(vrf_secret_key_data, 0, sizeof(vrf_secret_key_data));
  memset(vrf_public_key_data, 0, sizeof(vrf_public_key_data));
  if (create_random_VRF_keys((unsigned char *)vrf_public_key_data, (unsigned char *)vrf_secret_key_data) != 1 || crypto_vrf_is_valid_key((const unsigned char *)vrf_public_key_data) != 1)
  {
    COLOR_PRINT("Could not generate keys", "red");
    return;
  }
  // convert the VRF data to a string
  //for (count2 = 0, count = 0; count2 < (int)crypto_vrf_SECRETKEYBYTES; count2++, count += 2)
  //{
  //  snprintf(vrf_secret_key_hex + count, VRF_SECRET_KEY_LENGTH - 1, "%02x", vrf_secret_key_data[count2] & 0xFF);
  //}
  //for (count2 = 0, count = 0; count2 < (int)crypto_vrf_PUBLICKEYBYTES; count2++, count += 2)
  //{
  //  snprintf(vrf_public_key_hex + count, VRF_PUBLIC_KEY_LENGTH - 1, "%02x", vrf_public_key_data[count2] & 0xFF);
  //}

  for (count2 = 0, count = 0; count2 < crypto_vrf_SECRETKEYBYTES; count2++, count += 2) {
    snprintf(vrf_secret_key_hex + count, sizeof(vrf_secret_key_hex) - count, "%02x", vrf_secret_key_data[count2]);
  }
  for (count2 = 0, count = 0; count2 < crypto_vrf_PUBLICKEYBYTES; count2++, count += 2) {
    snprintf(vrf_public_key_hex + count, sizeof(vrf_public_key_hex) - count, "%02x", vrf_public_key_data[count2]);
  }

  COLOR_PRINT("\nPublic Key:", "green");
  COLOR_PRINT(vrf_public_key_hex, "green");
  COLOR_PRINT("\nSecret Key:", "green");
  COLOR_PRINT(vrf_secret_key_hex, "green");
  return;
}

/*---------------------------------------------------------------------------------------------------------
Name: VRF_sign_data
Description: Sign data using the block verifiers ECDSA key
Parameters:
  beta - The beta string
  proof - The proof
  data - The data
Return: 0 if an error has occurred, 1 if successful
---------------------------------------------------------------------------------------------------------*/
int VRF_sign_data(char *beta_string, char *proof, const char *data)
{
    // Validate input parameters
    if (!beta_string || !proof || !data) {
        ERROR_PRINT("Invalid input to VRF_sign_data.");
        return XCASH_ERROR;
    }

    // Variables
    unsigned char proof_data[crypto_vrf_PROOFBYTES] = {0};
    unsigned char beta_string_data[crypto_vrf_OUTPUTBYTES] = {0};

    // Clear output buffers explicitly
    memset(beta_string, 0, VRF_BETA_LENGTH + 1);
    memset(proof, 0, VRF_PROOF_LENGTH + 1);

    // Sign data
    if (crypto_vrf_prove(proof_data, (const unsigned char *)secret_key_data, (const unsigned char *)data, (unsigned long long)strlen(data)) != 0 ||
        crypto_vrf_proof_to_hash(beta_string_data, proof_data) != 0) {
        ERROR_PRINT("Failed to generate VRF proof or beta string.");
        return XCASH_ERROR;
    }

    // Convert proof and beta_string to hexadecimal format
    for (size_t i = 0; i < crypto_vrf_PROOFBYTES; i++) {
        snprintf(proof + (i * 2), 3, "%02x", proof_data[i]);
    }

    for (size_t i = 0; i < crypto_vrf_OUTPUTBYTES; i++) {
        snprintf(beta_string + (i * 2), 3, "%02x", beta_string_data[i]);
    }

    return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: sign_network_block_string
Description: Signs the network block string
Parameters:
  data - The signed data
  MESSAGE - The sign_data
  MESSAGE_SETTINGS - 1 to print the messages, otherwise 0. This is used for the testing flag to not print any 
  success or error messages
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int sign_network_block_string(char *data, const char *MESSAGE)
{
  // Variables
  char beta_string[SMALL_BUFFER_SIZE] = {0};
  char proof[SMALL_BUFFER_SIZE] = {0};

  // Sign the data using VRF
  if (VRF_sign_data(beta_string, proof, MESSAGE) == 0)
  {
    ERROR_PRINT("Could not sign the network block string");
    return XCASH_ERROR;
  }

  // Copy proof and beta string into the result
  memcpy(data, proof, VRF_PROOF_LENGTH);
  memcpy(data + VRF_PROOF_LENGTH, beta_string, VRF_BETA_LENGTH);

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: VRF_data_verify
Description: Verifies data
Parameters:
  PUBLIC_ADDRESS - The public address
  DATA_SIGNATURE - The data signature
  DATA - The data
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int VRF_data_verify(const char* BLOCK_VERIFIERS_PUBLIC_KEY, const char* BLOCK_VERIFIERS_DATA_SIGNATURE, const char* DATA)
{
  // Variables
  unsigned char public_key_data[crypto_vrf_PUBLICKEYBYTES] = {0};
  unsigned char proof_data[crypto_vrf_PROOFBYTES] = {0};
  unsigned char beta_string_data[crypto_vrf_OUTPUTBYTES] = {0};
  char hex_byte[3] = {0}; // Holds two hex chars + null terminator
  int i;

  // Convert public key hex string to bytes
  for (i = 0; i < VRF_PUBLIC_KEY_LENGTH; i += 2) {
    memcpy(hex_byte, &BLOCK_VERIFIERS_PUBLIC_KEY[i], 2);
    public_key_data[i / 2] = (unsigned char)strtol(hex_byte, NULL, 16);
  }

  // Convert proof hex string to bytes
  for (i = 0; i < VRF_PROOF_LENGTH; i += 2) {
    memcpy(hex_byte, &BLOCK_VERIFIERS_DATA_SIGNATURE[i], 2);
    proof_data[i / 2] = (unsigned char)strtol(hex_byte, NULL, 16);
  }

  // Convert beta string hex to bytes
  for (i = 0; i < VRF_BETA_LENGTH; i += 2) {
    memcpy(hex_byte, &BLOCK_VERIFIERS_DATA_SIGNATURE[VRF_PROOF_LENGTH + i], 2);
    beta_string_data[i / 2] = (unsigned char)strtol(hex_byte, NULL, 16);
  }

  // Verify
  return crypto_vrf_verify(beta_string_data, public_key_data, proof_data,
    (const unsigned char*)DATA, (unsigned long long)strlen(DATA)) == 0 ? XCASH_OK : XCASH_ERROR;
}