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
int check_reserve_proofs(uint64_t vote_amount_atomic, const char* PUBLIC_ADDRESS, const char* RESERVE_PROOF);
int get_unlocked_balance(uint64_t* unlocked_balance_out);
int wallet_payout_send(const char* addr, int64_t amount_atomic, const char* reason, char* first_tx_hash_out, size_t first_tx_hash_out_len,
  uint64_t* fee_out, int64_t* created_at_ms_out, uint64_t* amount_sent_out, char (*txids_out)[TRANSACTION_HASH_LENGTH + 1], 
  size_t txids_out_cap, size_t* tx_count_out);

#endif