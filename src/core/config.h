#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>

// ===================== XCASH Version =====================
#define XCASH_DPOPS_CURRENT_VERSION "xCash Labs DPoPs V. 2.0.0"
#define XCASH_DPOPS_CURRENT_VERSION_NUM "2.0.0"
#define XCASH_VERSION_LENGTH 15

// ===================== Blockchain Settings =====================
#define XCASH_WALLET_PREFIX "XCK" // The prefix of a XCK address
#define XCASH_SIGN_DATA_PREFIX "SigV2"
#define XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT 1
#define CRYPTONOTE_DISPLAY_DECIMAL_POINT 6
#define XCASH_ATOMIC_UNITS 1000000ULL  // 1 XCASH = 1,000,000 atomic units
#define MAX_SIBLINGS 15

// ===================== Network Ports and IPs =====================
#define XCASH_DAEMON_PORT 18281
#define XCASH_WALLET_PORT 18285
#define XCASH_DPOPS_PORT 18283
#define XCASH_DAEMON_IP "127.0.0.1"
#define XCASH_WALLET_IP "127.0.0.1"
#define XCASH_DPOPS_IP "127.0.0.1"
#define MAXIMUM_CONNECTIONS_IP_ADDRESS_OR_PUBLIC_ADDRESS 128
#define CONNECT_TIMEOUT_SEC 4
#define RECEIVE_TIMEOUT_SEC 5
#define SEND_TIMEOUT_MS 4000
#define NET_MULTI_WORKERS 12

// ===================== Network Block String =====================
#define EXTRA_NONCE_TAG "02"
#define TRANSACTION_PUBLIC_KEY_TAG "01"
#define TRANSACTION_HASH_LENGTH 64 // The length of the transaction hash
#define TX_EXTRA_VRF_SIGNATURE_TAG 0x07
#define BLOCK_RESERVED_SIZE 255
#define VRF_BLOB_TOTAL_SIZE 210
#define RR_IN 1
#define RR_TXT 16

// ===================== Buffer Sizes =====================
#define BUFFER_SIZE_NETWORK_BLOCK_DATA 500
#define VVSMALL_BUFFER_SIZE 512
#define VSMALL_BUFFER_SIZE 1024
#define SMALL_BUFFER_SIZE 2048
#define MEDIUM_BUFFER_SIZE 4096
#define LARGE_BUFFER_SIZE 8192
#define XLARGE_BUFFER_SIZE 16384
#define BUFFER_SIZE 300000
#define BUFFER_SIZE_RESERVE_PROOF 2560
#define MINIMUM_BUFFER_SIZE_DELEGATES_NAME 5
#define MAXIMUM_BUFFER_SIZE_DELEGATES_NAME 100

// ===================== Hash and Key Lengths =====================
#define DATA_HASH_LENGTH 128
#define XCASH_WALLET_LENGTH 98
#define BLOCK_HASH_LENGTH 64
#define RANDOM_STRING_LENGTH 100
#define XCASH_SIGN_DATA_LENGTH 93
#define VRF_PUBLIC_KEY_LENGTH 64
#define VRF_SECRET_KEY_LENGTH 128
#define VRF_RANDOMBYTES_LENGTH 32
#define VRF_PROOF_LENGTH 160
#define VRF_BETA_LENGTH 128
#define VRF_BETA_BYTES (VRF_BETA_LENGTH / 2)
#define SHA256_HASH_SIZE 32
#define SHA256_DIGEST_SIZE 64
#define DB_HASH_SIZE 128
#define BLOCK_HEIGHT_LENGTH 32
#define SIGNATURE_BIN_LEN 64
#define SYNC_TOKEN_LEN 32
#define VOTE_HASH_LEN 64

// ===================== Database =====================
#define DATABASE_CONNECTION "mongodb://127.0.0.1:27017"
#define DATABASE_NAME "XCASH_PROOF_OF_STAKE"
#define DB_COLLECTION_DELEGATES "delegates"
#define DB_COLLECTION_RESERVE_PROOFS "reserve_proofs"
#define DB_COLLECTION_STATISTICS "statistics"
#define DB_COLLECTION_ROUNDS "consensus_rounds"
#define DB_COLLECTION_BLOCKS_FOUND "blocks_found"
#define DB_COLLECTION_PAYOUT_BALANCES "payout_balances"
#define DB_COLLECTION_PAYOUT_RECEIPTS "payout_receipts"
#define DB_COLLECTION_APP_DELEGATES "approved_delegates"
#define DB_COLLECTION_NAME_SIZE 256
#define MAXIMUM_DATABASE_COLLECTION_DOCUMENTS 5000
#define DATABASE_EMPTY_STRING "empty_database_collection"
#define TOTAL_RESERVE_PROOFS_DATABASES 50
#define ID_MAX_SIZE 256
#define NUM_FIELDS 17
#define NUM_DB_FIELDS 16

// ===================== General Settings =====================
#define BITS_IN_BYTE 8
#define BLOCK_TIME 1
#define BLOCK_TIME_SEC (BLOCK_TIME*60)
#define SEND_PAYMENT_TIMEOUT_SETTINGS 500                    
#define BLOCK_TIMEOUT_SECONDS 10
#define HTTP_TIMEOUT_SETTINGS 4
#define DELAY_EARLY_TRANSACTIONS_MAX 2 // 2 seconds
#define NO_ACTIVITY_DELETE  (7LL * 24 * 60 * 60 * 1000LL)  // 7 day used for payouts
#define BLOCKS_PER_DAY 1440 // 1 min blocktimes
#define BLOCKS_BEHIND_CURRENT (BLOCKS_PER_DAY * 1) // Days behind

// ===================== XCASH LABS DPOPS =====================
#define BLOCK_VERIFIERS_TOTAL_AMOUNT 55
#define BLOCK_VERIFIERS_AMOUNT 50
#define BLOCK_VERIFIERS_SETTINGS 3
#define VOTE_PARAMETER_AMOUNT 5
#define REGISTER_PARAMETER_AMOUNT 6
#define UPDATE_PARAMETER_AMOUNT 5
#define GET_RESERVE_BYTES_DATABASE_HASH_PARAMETER_AMOUNT 5
#define NETWORK_DATA_NODES_VALID_AMOUNT 2
#define COMMITTEE_SIZE 10
#define SEED_COUNT 4
#define MAX_BANNED_IPS 20

// Need to relook at this
#define BLOCK_VERIFIERS_VALID_AMOUNT 5

#define MAXIMUM_TRANSACATIONS_PER_BLOCK 500
#define MAX_CONNECTIONS 1024
#define MAX_THREADS 10
#define MIN_VOTE_ATOMIC 4000000000000ULL
#define MAX_PROOFS_PER_DELEGATE_HARD 5000
#define ATOMIC_UNITS_PER_XCA 1000000LL
#define SAFE_CONFIRMATION_MARGIN 60
#define IP_LENGTH 255
#define DELEGATES_ONLINE_BUFFER ((IP_LENGTH * BLOCK_VERIFIERS_TOTAL_AMOUNT) + VSMALL_BUFFER_SIZE) // add buffer padding just incase
#define MINIMUM_BYTE_RANGE 1 // The minimum byte range to use when calculating the next block producer
#define MAXIMUM_BYTE_RANGE 250 // The maximum byte range to use when calculating the next block producer
#define BLOCK_VERIFIERS_CREATE_BLOCK_TIMEOUT_SETTINGS 5 // The time to wait to check if the block was created
#define SUBMIT_NETWORK_BLOCK_TIME_SECONDS 25 // The time to submit the network block
#define NETWORK_NODE_0 "xcashseeds_us" // Network node 0
#define PRODUCER_REF_COUNT 1  // Main + 0 backups for now
#define MAJORITY_PERCENT 70
#define SEED_REGISTRATION_TIME_UTC 1756684860ULL  // 2025-09-01 00:01:00 UTC

#define MAX_ACTIVE_CLIENTS 200

#define WAKEUP_SKEW_SEC 10 

#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW 60

// ===================== Constants =====================
#define ALPHANUM_STRING "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define XCASH_OK 1
#define XCASH_ERROR 0
#define BASE58_TMP_SIZE 128
#define BPS_SCALE 10000ULL

#endif // CONFIG_H_