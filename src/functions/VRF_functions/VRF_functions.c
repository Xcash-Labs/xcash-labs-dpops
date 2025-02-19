#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#include "macro_functions.h"
#include "config.h"
#include "convert.h"
#include "vrf.h"
#include "crypto_vrf.h"
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