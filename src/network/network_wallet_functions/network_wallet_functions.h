#ifndef NETWORK_WALLET_FUNCTIONS_H_   /* Include guard */
#define NETWORK_WALLET_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "network_functions.h"
#include "string_functions.h"

int get_public_address(void);
int check_reserve_proofs(uint64_t vote_atomic_amount, const char* PUBLIC_ADDRESS, const char* RESERVE_PROOF);

#endif