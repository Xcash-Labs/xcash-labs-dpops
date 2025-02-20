#ifndef DEFINE_CONFIG_H_   /* Include guard */
#define CONFIG_H_

#include <stdbool.h>

#define XCASH_DPOPS_CURRENT_VERSION "xCash Labs DPoPs V. 2.0.0"

#define XCASH_DAEMON_PORT 18281  // The X-CASH Daemon RPC port
#define XCASH_WALLET_PORT 18285  // The X-CASH Wallet RPC port
#define XCASH_DPOPS_PORT 18283  // The X-CASH Dpops service
#define TRANSFER_BUFFER_SIZE 4096  // Size of the buffer used for data transfer in bytes (4 KB)
#define RESPONSE_TIMEOUT 5000  // Maximum time (in milliseconds) to wait for a response before closing the connection (5 seconds)
#define CONNECTION_TIMEOUT 3000  // Maximum time (in milliseconds) to wait for a connection to be established before retrying or failing (3 seconds)
#define MAX_RETRIES 3  // Number of times a failed connection attempt will be retried before marking it as failed
#define RETRY_DELAY_MS 500  // Delay (in milliseconds) before retrying a failed connection attempt (0.5 seconds)


#define DATABASE_CONNECTION "mongodb://127.0.0.1:27017" // The database connection string

#define BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH 100 // The maximum length of the block verifiers IP address  ????
#define XCASH_WALLET_LENGTH 98 // The length of a XCA address
#define VRF_SECRET_KEY_LENGTH 128 // Length of VRF Secret Key

// Lengths
#define IP_LENGTH 39
#define BUFFER_SIZE_NETWORK_BLOCK_DATA 500
#define BUFFER_SIZE 300000
#define SMALL_BUFFER_SIZE 2000

// VRF
#define VRF_PUBLIC_KEY_LENGTH 64
#define VRF_SECRET_KEY_LENGTH 128
#define VRF_PROOF_LENGTH 160

#define XCASH_OK 1
#define XCASH_ERROR 0

#define BLOCK_TIME 5 // The block time in minutes
#define BLOCK_TIME_SEC (BLOCK_TIME*60) // The block time in seconds

#define SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS 3 // The time to wait for sending or receving socket data


#endif