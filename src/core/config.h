#ifndef DEFINE_CONFIG_H_   /* Include guard */
#define CONFIG_H_

#include <stdbool.h>

#define XCASH_DPOPS_CURRENT_VERSION "xCash Labs DPoPs V. 2.0.0"

#define XCASH_DAEMON_PORT 18281  // The X-CASH Daemon RPC port
#define XCASH_WALLET_PORT 18285  // The X-CASH Wallet RPC port
#define XCASH_DPOPS_PORT 18283  // The X-CASH Dpops service
#define MAXIMUM_CONNECTIONS 1024  // P2P best practice: handle high concurrency                look at this later
#define BACKLOG_SIZE 128  // Fine-tuned backlog queue for P2P

#define DATABASE_CONNECTION "mongodb://127.0.0.1:27017" // The database connection string

#define BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH 100 // The maximum length of the block verifiers IP address  ????
#define XCASH_WALLET_LENGTH 98 // The length of a XCA address
#define VRF_SECRET_KEY_LENGTH 128 // Length of VRF Secret Key

// Lengths
#define IP_LENGTH 39
#define SMALL_BUFFER_SIZE 2000

// VRF
#define VRF_PUBLIC_KEY_LENGTH 64
#define VRF_SECRET_KEY_LENGTH 128
#define VRF_PROOF_LENGTH 160

#endif