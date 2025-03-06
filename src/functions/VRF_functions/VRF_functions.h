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

void generate_key(void);
int VRF_sign_data(char *beta_string, char *proof, const char* data);

#endif