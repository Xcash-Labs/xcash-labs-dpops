#ifndef CONFIG_H_   /* Include guard */
#define CONFIG_H_

#include <stdbool.h>

#define XCASH_DPOPS_CURRENT_VERSION "xCash Labs DPoPs V. 2.0.0"
#define XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT 2 // The start block height for X-CASH proof of stake

// Network
#define XCASH_DAEMON_PORT 18281 // The X-CASH Daemon RPC Port
#define XCASH_WALLET_PORT 18285 // The X-CASH Wallet RPC Port
#define XCASH_DPOPS_PORT 18283 // The X-CASH Dpops Port
#define XCASH_DAEMON_IP "127.0.0.1" // The X-CASH Wallet IP
#define XCASH_WALLET_IP "127.0.0.1" // The X-CASH Wallet IP
#define XCASH_DPOPS_IP "127.0.0.1" // The X-CASH Wallet IP
#define SOCKET_END_STRING "|END|" // End string when sending data between nodes, to signal the end of sending data

#define XCASH_WALLET_PREFIX "XCA" // The prefix of a XCA address
#define XCASH_SIGN_DATA_PREFIX "SigV1" // The prefix of a XCASH_DPOPS_signature for the signed data

#define TRANSFER_BUFFER_SIZE 4096  // Size of the buffer used for data transfer in bytes (4 KB)
#define RESPONSE_TIMEOUT 5000  // Maximum time (in milliseconds) to wait for a response before closing the connection (5 seconds)
#define CONNECTION_TIMEOUT 3000  // Maximum time (in milliseconds) to wait for a connection to be established before retrying or failing (3 seconds)
#define MAX_RETRIES 3  // Number of times a failed connection attempt will be retried before marking it as failed
#define RETRY_DELAY_MS 500  // Delay (in milliseconds) before retrying a failed connection attempt (0.5 seconds)
#define MAX_CONNECTIONS 1024 // Max connection for incomming transactions

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

// Lengths
#define IP_LENGTH 253
#define BUFFER_SIZE_NETWORK_BLOCK_DATA 500
#define BUFFER_SIZE 300000
#define SMALL_BUFFER_SIZE 2000
#define MAXIMUM_BUFFER_SIZE 52428800 // 50 MB                                   ???????????????????????????
#define DATA_HASH_LENGTH 128 // The length of the SHA2-512 hash
#define XCASH_PUBLIC_ADDR_LENGTH 98 // The length of a XCA address
#define XCASH_WALLET_LENGTH 98 // The length of a XCA addres
#define BLOCK_HASH_LENGTH 64 // The length of the block hash
#define RANDOM_STRING_LENGTH 100 // The length of the random string
#define XCASH_SIGN_DATA_LENGTH 93 // The length of a XCASH_DPOPS_signature for the signed data

#define BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH 100 // The maximum length of the block verifiers IP address  ????
#define MD5_HASH_SIZE 32
#define BLOCK_VERIFIERS_VALID_AMOUNT 3 // The amount of block verifiers that need to vote true for the part of the round to be valid

// VRF
#define VRF_PUBLIC_KEY_LENGTH 64
#define VRF_SECRET_KEY_LENGTH 128
#define VRF_PROOF_LENGTH 160
#define VRF_BETA_LENGTH 128

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


#define ALPHANUM_STRING "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

typedef enum XCASH_DBS {
    XCASH_DB_DELEGATES = 0,
    XCASH_DB_STATISTICS = 1,
    XCASH_DB_RESERVE_PROOFS = 2,
    XCASH_DB_RESERVE_BYTES = 3,
    XCASH_DB_COUNT
  } xcash_dbs_t;

#endif