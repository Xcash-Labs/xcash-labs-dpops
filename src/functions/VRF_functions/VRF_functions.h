#ifndef VRF_FUNCTIONS_H_   /* Include guard */
#define VRF_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "convert.h"
#include "vrf.h"
#include "crypto_vrf.h"

int create_random_VRF_keys(unsigned char *public_key, unsigned char *secret_key);
void generate_key(void);
int sign_network_block_string(char *data, const char* MESSAGE);
int VRF_sign_data(char *beta_string, char *proof, const char* data);
int VRF_data_verify(const char* BLOCK_VERIFIERS_PUBLIC_KEY, const char* BLOCK_VERIFIERS_DATA_SIGNATURE, const char* DATA)

#endif