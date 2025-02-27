#ifndef DEFINE_CONFIG_H_   /* Include guard */
#define CONFIG_H_

#include <stdbool.h>

#define XCASH_DPOPS_CURRENT_VERSION "xCash Labs DPoPs V. 2.0.0"

#define XCASH_DAEMON_PORT 18281 // The X-CASH Daemon RPC IP
#define XCASH_WALLET_PORT 18285 // The X-CASH Wallet RPC IP
#define XCASH_DPOPS_PORT 18283 // The X-CASH Dpops IP
#define DATABASE_CONNECTION "mongodb://127.0.0.1:27017" // The database connection string

#define XCASH_WALLET_PREFIX "XCA" // The prefix of a XCA address

#define TRANSFER_BUFFER_SIZE 4096  // Size of the buffer used for data transfer in bytes (4 KB)
#define RESPONSE_TIMEOUT 5000  // Maximum time (in milliseconds) to wait for a response before closing the connection (5 seconds)
#define CONNECTION_TIMEOUT 3000  // Maximum time (in milliseconds) to wait for a connection to be established before retrying or failing (3 seconds)
#define MAX_RETRIES 3  // Number of times a failed connection attempt will be retried before marking it as failed
#define RETRY_DELAY_MS 500  // Delay (in milliseconds) before retrying a failed connection attempt (0.5 seconds)
#define MAX_CONNECTIONS 1024 // Max connection for incomming transactions

//database
#define DATABASE_NAME "XCASH_PROOF_OF_STAKE" // The name of the database
#define DATABASE_NAME_DELEGATES "XCASH_PROOF_OF_STAKE_DELEGATES" // The name of the database for the delegates
#define MAXIMUM_DATABASE_WRITE_SIZE 48000000 // The maximum database write size
#define DATABASE_TOTAL 4 // The amount of databases

// Lengths
#define IP_LENGTH 253
#define BUFFER_SIZE_NETWORK_BLOCK_DATA 500
#define BUFFER_SIZE 300000
#define SMALL_BUFFER_SIZE 2000
#define DATA_HASH_LENGTH 128 // The length of the SHA2-512 hash
#define XCASH_PUBLIC_ADDR_LENGTH 98 // The length of a XCA address
#define VRF_SECRET_KEY_LENGTH 128 // Length of VRF Secret Key
#define XCASH_WALLET_LENGTH 98 // The length of a XCA addres
#define BLOCK_HASH_LENGTH 64 // The length of the block hash

#define BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH 100 // The maximum length of the block verifiers IP address  ????
#define VRF_SECRET_KEY_LENGTH 128 // Length of VRF Secret Key
#define MD5_HASH_SIZE 32
#define BLOCK_VERIFIERS_VALID_AMOUNT 3 // The amount of block verifiers that need to vote true for the part of the round to be valid

// VRF
#define VRF_PUBLIC_KEY_LENGTH 64
#define VRF_SECRET_KEY_LENGTH 128
#define VRF_PROOF_LENGTH 160

#define XCASH_OK 1
#define XCASH_ERROR 0

#define BLOCK_TIME 5 // The block time in minutes
#define BLOCK_TIME_SEC (BLOCK_TIME*60) // The block time in seconds

#define SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS 3 // The time to wait for sending or receving socket data
#define INVALID_RESERVE_PROOFS_SETTINGS 3 // The time in seconds to wait between checking for invalid reserve proofs

#endif