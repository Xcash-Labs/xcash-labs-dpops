#ifndef CONFIG_H_   /* Include guard */
#define CONFIG_H_

#include <stdbool.h>

#define XCASH_DPOPS_CURRENT_VERSION "xCash Labs DPoPs V. 2.0.0"
#define XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT 2 // The start block height for X-CASH proof of stake

// Blockchain
#define XCASH_WALLET_PREFIX "XCA" // The prefix of a XCA address
#define XCASH_TOTAL_SUPPLY 100000000000 // The total supply
#define XCASH_PREMINE_TOTAL_SUPPLY 40000000000 // The total supply of the premine
#define FIRST_BLOCK_MINING_REWARD 190734.863281 // The first blocks mining reward
#define XCASH_SIGN_DATA_PREFIX "SigV1" // The prefix of a XCASH_DPOPS_signature for the signed data
#define XCASH_EMMISION_FACTOR 524288 // The emmision factor (2^19)
#define XCASH_DPOPS_EMMISION_FACTOR 262144 // The xcash-dpops emmision factor (2^18)
#define BLOCKCHAIN_RESERVED_BYTES_LENGTH_TEXT "7c424c4f434b434841494e5f52455345525645445f42595445535f4c454e4754487c"
#define BLOCKCHAIN_EXTRA_BYTES_LENGTH_TEXT "7c424c4f434b434841494e5f45585452415f42595445535f4c454e4754487c"
#define UNLOCK_BLOCK_AMOUNT 60 // The default unlock block amount for a block reward transaction

// Network
#define XCASH_DAEMON_PORT 18281 // The X-CASH Daemon RPC Port
#define XCASH_WALLET_PORT 18285 // The X-CASH Wallet RPC Port
#define XCASH_DPOPS_PORT 18283 // The X-CASH Dpops Port
#define XCASH_DAEMON_IP "127.0.0.1" // The X-CASH Wallet IP
#define XCASH_WALLET_IP "127.0.0.1" // The X-CASH Wallet IP
#define XCASH_DPOPS_IP "127.0.0.1" // The X-CASH Wallet IP
#define MAXIMUM_CONNECTIONS_IP_ADDRESS_OR_PUBLIC_ADDRESS 20 // The maximum connections a specific IP address or specific public address can have at one time

// Network block string
#define BLOCKCHAIN_DATA_SEGMENT_PUBLIC_ADDRESS_STRING_DATA "7c424c4f434b434841494e5f444154415f5345474d454e545f535452494e475f5055424c49435f4b45597c"
#define BLOCKCHAIN_DATA_SEGMENT_STRING "7c424c4f434b434841494e5f444154415f5345474d454e545f535452494e477c"
#define NETWORK_VERSION "0d0d" // The network version
#define TIMESTAMP_LENGTH 10 // The length of the timestamp
#define BLOCK_REWARD_INPUT "01"
#define BLOCK_REWARD_TRANSACTION_VERSION "02"
#define VIN_TYPE "ff"
#define BLOCK_REWARD_OUTPUT "01"
#define STEALTH_ADDRESS_OUTPUT_LENGTH 64 // The length of the stealth address output
#define BLOCKCHAIN_RESERVED_BYTES_START "7c424c4f434b434841494e5f52455345525645445f42595445535f53544152547c"
#define BLOCKCHAIN_RESERVED_BYTES_END "7c424c4f434b434841494e5f52455345525645445f42595445535f454e447c"
#define VARINT_DECODED_VALUE_END_2_BYTE 2097151 // end of 2 byte length
#define BLOCKCHAIN_RESERVED_BYTES_DATA_HASH "02800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" // the reserve bytes data hash
#define STEALTH_ADDRESS_OUTPUT_TAG "02"
#define TRANSACTION_PUBLIC_KEY_LENGTH 66 // The length of the transaction public key
#define EXTRA_NONCE_TAG "02"
#define TRANSACTION_PUBLIC_KEY_TAG "01"
#define RINGCT_VERSION "00"
#define TRANSACTION_LENGTH 64 // The length of a transaction
#define CONSENSUS_NODE_NETWORK_BLOCK_NONCE "11111111" // The network block nonce used when the consensus node creates the block
#define VARINT_DECODED_VALUE_END_4_BYTE 34359738367 // end of 4 byte length
#define VARINT_DECODED_VALUE_START_7_BYTE 562949953421312 // start of 7 byte length
#define VARINT_ENCODED_VALUE_END_7_BYTE 0xffffffffffffff7f // end of 7 byte length
#define VARINT_DECODED_VALUE_END_1_BYTE 16383 // end of 1 byte length




#define TRANSFER_BUFFER_SIZE 4096  // Size of the buffer used for data transfer in bytes (4 KB)
#define RESPONSE_TIMEOUT 5000  // Maximum time (in milliseconds) to wait for a response before closing the connection (5 seconds)
#define SEND_TIMEOUT 5000  // Maximum time (in milliseconds) to wait for a response before closing the connection (5 seconds)
#define CONNECTION_TIMEOUT 3000  // Maximum time (in milliseconds) to wait for a connection to be established before retrying or failing (3 seconds)
#define MAX_CONNECTIONS 1024 // Max connection for incomming transactions
#define MAX_THREADS 10 // Max threads to use

// Database 
#define DATABASE_CONNECTION "mongodb://127.0.0.1:27017" // The database connection string
#define DATABASE_NAME "XCASH_PROOF_OF_STAKE" // The name of the database
#define DATABASE_NAME_DELEGATES "XCASH_PROOF_OF_STAKE_DELEGATES" // The name of the database for the delegates
#define MAXIMUM_DATABASE_WRITE_SIZE 48000000 // The maximum database write size
#define DATABASE_TOTAL 4 // The amount of databases
#define TOTAL_DELEGATES_DATABASE_FIELDS 18 // The total delegates database fields
#define MAXIMUM_DATABASE_COLLECTION_DOCUMENTS 5000 // The maximum amount of documents in a database collection
#define DATABASE_EMPTY_STRING "empty_database_collection" // The database data to give when the database collection is empty
#define TOTAL_RESERVE_PROOFS_DATABASES 50 // The total reserve proofs databases
#define DB_HASH_SIZE 128
#define ID_MAX_SIZE 256 //VRF_PUBLIC_KEY_LENGTH + 64*'0' + \0 + align just in case
#define DB_COLLECTION_DELEGATES "delegates"

#define MAXIMUM_TRANSACATIONS_PER_BLOCK 500 // The maximum amount of transaction per block

// Lengths
#define IP_LENGTH 253
#define BUFFER_SIZE_NETWORK_BLOCK_DATA 500
#define BUFFER_SIZE 300000
#define SMALL_BUFFER_SIZE 2048
#define MAXIMUM_BUFFER_SIZE 52428800 // 50 MB                                   ???????????????????????????
#define BITS_IN_BYTE 8 // 8 bits in 1 byte

#define DATA_HASH_LENGTH 128 // The length of the SHA2-512 hash
#define XCASH_PUBLIC_ADDR_LENGTH 98 // The length of a XCA address
#define XCASH_WALLET_LENGTH 98 // The length of a XCA addres
#define BLOCK_HASH_LENGTH 64 // The length of the block hash
#define RANDOM_STRING_LENGTH 100 // The length of the random string
#define XCASH_SIGN_DATA_LENGTH 93 // The length of a XCASH_DPOPS_signature for the signed data
#define MINIMUM_BUFFER_SIZE_DELEGATES_NAME 5 // The minimum length of the block verifiers name
#define MINIMUM_BUFFER_SIZE_DELEGATES_BACKUP_NAMES 30 // The minimum length of the block verifiers name


#define BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH 100 // The maximum length of the block verifiers IP address
#define MD5_HASH_SIZE 32
#define BLOCK_VERIFIERS_VALID_AMOUNT 3 // The amount of block verifiers that need to vote true for the part of the round to be valid

// VRF
#define VRF_PUBLIC_KEY_LENGTH 64
#define VRF_SECRET_KEY_LENGTH 128
#define VRF_PROOF_LENGTH 160
#define VRF_BETA_LENGTH 128
#define VRF_DATA "74727565" // true when the VRF data is verified


#define XCASH_OK 1
#define XCASH_ERROR 0
#define DB_COLLECTION_NAME_SIZE 256
#define NUM_FIELDS 18

#define BLOCK_TIME 5 // The block time in minutes
#define BLOCK_TIME_SEC (BLOCK_TIME*60) // The block time in seconds

#define SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS 3 // The time to wait for sending or receving socket data
#define INVALID_RESERVE_PROOFS_SETTINGS 3 // The time in seconds to wait between checking for invalid reserve proofs

// XCASH DPOPS
#define BLOCK_VERIFIERS_TOTAL_AMOUNT 100 // The total amount of block verifiers
#define BLOCK_VERIFIERS_AMOUNT 50 // The amount of block verifiers in a round
#define MAXIMUM_BUFFER_SIZE_DELEGATES_NAME 100 // The maximum length of the block verifiers name
#define BUFFER_SIZE_BLOCK_HEIGHTS_DATA 150000
#define VOTE_PARAMETER_AMOUNT 5 // The vote parameter amount
#define REGISTER_PARAMETER_AMOUNT 6 // The register parameter amount
#define UPDATE_PARAMETER_AMOUNT 5 // The update parameter amount
#define GET_RESERVE_BYTES_DATABASE_HASH_PARAMETER_AMOUNT 5 // The GET_RESERVE_BYTES_DATABASE_HASH parameter amount
#define MAXIMUM_NUMBER_SIZE 25 // The maximum amount of bytes a number could take up in X-CASH
#define BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME 288 // The blocks per day with a 5 minute block time
#define MAXIMUM_AMOUNT_OF_DELEGATES 150 // The maximum amount of delegates that can be registered
#define BLOCK_VERIFIERS_SETTINGS 3 // The time in seconds to wait to send data to the block verifiers
#define NETWORK_DATA_NODES_VALID_AMOUNT 2 // The amount of network data nodes need to reach a consensus on the database data

// Network block string 
#define BLOCK_PRODUCER_NETWORK_BLOCK_NONCE "00000000" // The network block nonce used when the block producer creates the block
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE_DATA "303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030" // The place holder bytes for a block verifier that does not create a block verifier signature, for the block verifier signature data
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA "30303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030" // The place holder bytes for a block verifier that does not create a VRF secret key, for the VRF secret key data
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" // The place holder bytes for a block verifier that does not create a block verifier signature
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY "0000000000000000000000000000000000000000000000000000000000000000" // The place holder bytes for a block verifier that does not create a VRF secret key
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING_DATA "30303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030" // The place holder bytes for a block verifier that does not create a random string, for the random string data
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" // The place holder bytes for a block verifier that does not create a random string
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA "3030303030303030303030303030303030303030303030303030303030303030" // The place holder bytes for a block verifier that does not create a VRF public key, for the VRF public key data
#define GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY "00000000000000000000000000000000" // The place holder bytes for a block verifier that does not create a VRF public key


#define GET_RESERVE_BYTES_DATABASE_HASH_PARAMETER_AMOUNT 5 // The GET_RESERVE_BYTES_DATABASE_HASH parameter amount

#define ALPHANUM_STRING "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

typedef enum XCASH_DBS {
    XCASH_DB_DELEGATES = 0,
    XCASH_DB_STATISTICS = 1,
    XCASH_DB_RESERVE_PROOFS = 2,
    XCASH_DB_RESERVE_BYTES = 3,
    XCASH_DB_COUNT
  } xcash_dbs_t;

#endif