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
  unsigned char vrf_secret_key_data[crypto_vrf_SECRETKEYBYTES + 1];
  unsigned char vrf_public_key_data[crypto_vrf_PUBLICKEYBYTES + 1];
  char vrf_secret_key[VRF_SECRET_KEY_LENGTH + SMALL_BUFFER_SIZE];
  char vrf_public_key[VRF_PUBLIC_KEY_LENGTH + SMALL_BUFFER_SIZE];
  int count;
  int count2;
  memset(vrf_secret_key, 0, sizeof(vrf_secret_key));
  memset(vrf_public_key, 0, sizeof(vrf_public_key));
  memset(vrf_secret_key_data, 0, sizeof(vrf_secret_key_data));
  memset(vrf_public_key_data, 0, sizeof(vrf_public_key_data));
  if (create_random_VRF_keys((unsigned char *)vrf_public_key_data, (unsigned char *)vrf_secret_key_data) != 1 || crypto_vrf_is_valid_key((const unsigned char *)vrf_public_key_data) != 1)
  {
    COLOR_PRINT("Could not generate keys", "red");
    return;
  }
  // convert the VRF data to a string
  for (count2 = 0, count = 0; count2 < (int)crypto_vrf_SECRETKEYBYTES; count2++, count += 2)
  {
    snprintf(vrf_secret_key + count, VRF_SECRET_KEY_LENGTH - 1, "%02x", vrf_secret_key_data[count2] & 0xFF);
  }
  for (count2 = 0, count = 0; count2 < (int)crypto_vrf_PUBLICKEYBYTES; count2++, count += 2)
  {
    snprintf(vrf_public_key + count, VRF_PUBLIC_KEY_LENGTH - 1, "%02x", vrf_public_key_data[count2] & 0xFF);
  }
  COLOR_PRINT("\nPublic Key:", "green");
  COLOR_PRINT(vrf_public_key, "green");
  COLOR_PRINT("\nSecret Key:", "green");
  COLOR_PRINT(vrf_secret_key, "green");
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