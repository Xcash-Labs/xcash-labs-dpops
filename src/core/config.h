#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>

// ===================== XCASH Version =====================
#define XCASH_DPOPS_CURRENT_VERSION "xCash Labs DPoPs V. 2.0.0"

// ===================== Blockchain Settings =====================
#define XCASH_WALLET_PREFIX "XCA"
#define XCASH_TOTAL_SUPPLY 100000000000
#define XCASH_PREMINE_TOTAL_SUPPLY 40000000000
#define FIRST_BLOCK_MINING_REWARD 190734.863281
#define XCASH_SIGN_DATA_PREFIX "SigV1"
#define XCASH_EMMISION_FACTOR 524288
#define XCASH_DPOPS_EMMISION_FACTOR 262144
#define XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT 2
#define UNLOCK_BLOCK_AMOUNT 60

// ===================== Blockchain Data Lengths =====================
#define BLOCKCHAIN_RESERVED_BYTES_LENGTH_TEXT "7c424c4f434b434841494e5f52455345525645445f42595445535f4c454e4754487c"
#define BLOCKCHAIN_EXTRA_BYTES_LENGTH_TEXT "7c424c4f434b434841494e5f45585452415f42595445535f4c454e4754487c"
#define BLOCKCHAIN_DATA_SEGMENT_STRING "7c424c4f434b434841494e5f444154415f5345474d454e545f535452494e477c"
#define BLOCKCHAIN_DATA_SEGMENT_PUBLIC_ADDRESS_STRING_DATA "7c424c4f434b434841494e5f444154415f5345474d454e545f535452494e475f5055424c49435f4b45597c"
#define BLOCKCHAIN_RESERVED_BYTES_START "7c424c4f434b434841494e5f52455345525645445f42595445535f53544152547c"
#define BLOCKCHAIN_RESERVED_BYTES_END "7c424c4f434b434841494e5f52455345525645445f42595445535f454e447c"
#define BLOCKCHAIN_RESERVED_BYTES_DATA_HASH "02800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"

// Blockchain field lengths
// ===================== Blockchain Field Lengths =====================
#define STEALTH_ADDRESS_OUTPUT_LENGTH 64  // Length of the stealth address output
#define TRANSACTION_PUBLIC_KEY_LENGTH 66  // Length of the transaction public key
#define TRANSACTION_LENGTH 64             // Length of a transaction
#define TIMESTAMP_LENGTH 10               // Length of the timestamp

// ===================== Network Ports and IPs =====================
#define XCASH_DAEMON_PORT 18281
#define XCASH_WALLET_PORT 18285
#define XCASH_DPOPS_PORT 18283
#define XCASH_DAEMON_IP "127.0.0.1"
#define XCASH_WALLET_IP "127.0.0.1"
#define XCASH_DPOPS_IP "127.0.0.1"
#define MAXIMUM_CONNECTIONS_IP_ADDRESS_OR_PUBLIC_ADDRESS 20

// ===================== Network Block String =====================
#define NETWORK_VERSION "0d0d"
#define BLOCK_REWARD_INPUT "01"
#define BLOCK_REWARD_TRANSACTION_VERSION "02"
#define VIN_TYPE "ff"
#define BLOCK_REWARD_OUTPUT "01"
#define STEALTH_ADDRESS_OUTPUT_TAG "02"
#define EXTRA_NONCE_TAG "02"
#define TRANSACTION_PUBLIC_KEY_TAG "01"
#define RINGCT_VERSION "00"
#define CONSENSUS_NODE_NETWORK_BLOCK_NONCE "11111111"
#define BLOCK_PRODUCER_NETWORK_BLOCK_NONCE "00000000"

// ======================== Placeholders for Block Template =========================
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE_DATA "303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030"
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA "30303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030"
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY "0000000000000000000000000000000000000000000000000000000000000000"
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING_DATA "30303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030"
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA "3030303030303030303030303030303030303030303030303030303030303030"
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY "00000000000000000000000000000000"
#define BLOCK_VERIFIER_MAJORITY_VRF_DATA_TEMPLATE "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
#define BLOCK_VERIFIER_MAJORITY_BLOCK_VERIFIERS_SIGNATURE_TEMPLATE "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"

// ===================== Buffer Sizes =====================
#define IP_LENGTH 253
#define BUFFER_SIZE_NETWORK_BLOCK_DATA 500
#define BUFFER_SIZE 300000
#define SMALL_BUFFER_SIZE 2048
#define MAXIMUM_BUFFER_SIZE 52428800
#define TRANSFER_BUFFER_SIZE 4096
#define MAXIMUM_NUMBER_SIZE 25
#define MAXIMUM_BUFFER_SIZE_DELEGATES_NAME 100
#define MAXIMUM_BUFFER_SIZE_DELEGATES_BACKUP_NAMES 505
#define MINIMUM_BUFFER_SIZE_DELEGATES_NAME 5
#define MINIMUM_BUFFER_SIZE_DELEGATES_BACKUP_NAMES 30
#define BUFFER_SIZE_BLOCK_HEIGHTS_DATA 150000
#define RESPONSE_TIMEOUT 5000
#define SEND_TIMEOUT 5000
#define CONNECTION_TIMEOUT 3000

// ===================== Hash and Key Lengths =====================
#define DATA_HASH_LENGTH 128
#define XCASH_PUBLIC_ADDR_LENGTH 98
#define XCASH_WALLET_LENGTH 98
#define BLOCK_HASH_LENGTH 64
#define RANDOM_STRING_LENGTH 100
#define XCASH_SIGN_DATA_LENGTH 93
#define VRF_PUBLIC_KEY_LENGTH 64
#define VRF_SECRET_KEY_LENGTH 128
#define VRF_PROOF_LENGTH 160
#define VRF_BETA_LENGTH 128
#define VRF_DATA "74727565"
#define MD5_HASH_SIZE 32
#define DB_HASH_SIZE 128

// ===================== Blockchain Varint Lengths =====================
#define VARINT_DECODED_VALUE_END_1_BYTE 16383
#define VARINT_DECODED_VALUE_END_2_BYTE 2097151
#define VARINT_DECODED_VALUE_END_4_BYTE 34359738367
#define VARINT_DECODED_VALUE_START_6_BYTE 4398046511104
#define VARINT_DECODED_VALUE_START_7_BYTE 562949953421312
#define VARINT_DECODED_VALUE_END_7_BYTE 72057594037927935
#define VARINT_ENCODED_VALUE_END_7_BYTE 0xffffffffffffff7f

// ===================== Database =====================
#define DATABASE_CONNECTION "mongodb://127.0.0.1:27017"
#define DATABASE_NAME "XCASH_PROOF_OF_STAKE"
#define DATABASE_NAME_DELEGATES "XCASH_PROOF_OF_STAKE_DELEGATES"
#define DB_COLLECTION_DELEGATES "delegates"
#define DB_COLLECTION_NAME_SIZE 256
#define MAXIMUM_DATABASE_WRITE_SIZE 48000000
#define DATABASE_TOTAL 4
#define TOTAL_DELEGATES_DATABASE_FIELDS 18
#define MAXIMUM_DATABASE_COLLECTION_DOCUMENTS 5000
#define DATABASE_EMPTY_STRING "empty_database_collection"
#define TOTAL_RESERVE_PROOFS_DATABASES 50
#define ID_MAX_SIZE 256
#define NUM_FIELDS 18

// ===================== General Settings =====================
#define BITS_IN_BYTE 8
#define BLOCK_TIME 5
#define BLOCK_TIME_SEC (BLOCK_TIME*60)
#define BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME 288
#define SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS 3
#define INVALID_RESERVE_PROOFS_SETTINGS 3

// ===================== XCASH DPOPS =====================
#define BLOCK_VERIFIERS_TOTAL_AMOUNT 100
#define BLOCK_VERIFIERS_AMOUNT 50
#define BLOCK_VERIFIERS_SETTINGS 3
#define VOTE_PARAMETER_AMOUNT 5
#define REGISTER_PARAMETER_AMOUNT 6
#define UPDATE_PARAMETER_AMOUNT 5
#define GET_RESERVE_BYTES_DATABASE_HASH_PARAMETER_AMOUNT 5
#define NETWORK_DATA_NODES_VALID_AMOUNT 2
#define BLOCK_VERIFIERS_VALID_AMOUNT 3
#define MAXIMUM_AMOUNT_OF_DELEGATES 150
#define MAXIMUM_TRANSACATIONS_PER_BLOCK 500
#define MAX_CONNECTIONS 1024
#define MAX_THREADS 10
#define BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH 100
#define MINIMUM_BYTE_RANGE 1 // The minimum byte range to use when calculating the next block producer
#define MAXIMUM_BYTE_RANGE 250 // The maximum byte range to use when calculating the next block producer
#define BLOCK_VERIFIERS_CREATE_BLOCK_TIMEOUT_SETTINGS 5 // The time to wait to check if the block was created
#define SUBMIT_NETWORK_BLOCK_TIME_SECONDS 25 // The time to submit the network block
#define NETWORK_NODE_0 "xcashseeds_us" // Network node 0

// ===================== Constants =====================
#define ALPHANUM_STRING "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define XCASH_OK 1
#define XCASH_ERROR 0

// ===================== Enums =====================
typedef enum XCASH_DBS {
    XCASH_DB_DELEGATES = 0,
    XCASH_DB_STATISTICS = 1,
    XCASH_DB_RESERVE_PROOFS = 2,
    XCASH_DB_RESERVE_BYTES = 3,
    XCASH_DB_COUNT
} xcash_dbs_t;

#endif // CONFIG_H_