#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stddef.h>

typedef struct {
    char *block_verifiers_secret_key;
    bool delegates_website;
    bool shared_delegates_website;
    uint64_t minimum_amount;
} arg_config_t;

// Define a struct to store network node data
typedef struct {
    const char *seed_public_address;
    const char *ip_address;
    const char *seed_public_key;
    int online_status;
} NetworkNode;

typedef struct {
    const char *ip_address;
    bool dsfound;
} xCashpulseNode;

typedef struct {
  char version[32];
  char digest[SHA256_DIGEST_SIZE + 1];
} updpops_entry_t;

// xcash-next
typedef struct  {
    char public_address[XCASH_WALLET_LENGTH+1];
    uint64_t total_vote_count;
    char IP_address[IP_LENGTH+1];
    char delegate_name[MAXIMUM_BUFFER_SIZE_DELEGATES_NAME+1];
    char about[512];
    char website[256];
    char team[256];
    char delegate_type[10];
    float delegate_fee;
    char server_specs[256];
    char online_status[11];
    char public_key[VRF_PUBLIC_KEY_LENGTH+1];
    uint64_t registration_timestamp;
    char online_status_orginal[11];
    char verifiers_vrf_proof_hex[VRF_PROOF_LENGTH + 1];
    char verifiers_vrf_beta_hex[VRF_BETA_LENGTH + 1];
} delegates_t;

typedef struct {
    char public_address[XCASH_WALLET_LENGTH+1];
    char IP_address[IP_LENGTH+1];
} delegates_timer_t;

typedef struct {
  mongoc_client_pool_t *pool;
} sched_ctx_t;

typedef struct {
    char block_verifiers_name[BLOCK_VERIFIERS_AMOUNT][MAXIMUM_BUFFER_SIZE_DELEGATES_NAME+1]; // The block verifiers name
    char block_verifiers_public_address[BLOCK_VERIFIERS_AMOUNT][XCASH_WALLET_LENGTH+1]; // The block verifiers public address
    char block_verifiers_public_key[BLOCK_VERIFIERS_AMOUNT][VRF_PUBLIC_KEY_LENGTH+1]; // The block verifiers public key
    char block_verifiers_IP_address[BLOCK_VERIFIERS_AMOUNT][IP_LENGTH+1]; // The block verifiers IP address
    char block_verifiers_vrf_proof_hex[BLOCK_VERIFIERS_AMOUNT][VRF_PROOF_LENGTH + 1];
    char block_verifiers_vrf_beta_hex[BLOCK_VERIFIERS_AMOUNT][VRF_BETA_LENGTH + 1];
    int block_verifiers_vote_total[BLOCK_VERIFIERS_AMOUNT];
    uint8_t block_verifiers_voted[BLOCK_VERIFIERS_AMOUNT];
    char block_verifiers_vote_signature[BLOCK_VERIFIERS_AMOUNT][XCASH_SIGN_DATA_LENGTH + 1];
    char block_verifiers_selected_public_address[BLOCK_VERIFIERS_AMOUNT][XCASH_WALLET_LENGTH+1];
} block_verifiers_list_t;

typedef enum {
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA,
    XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS,
    XMSG_NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST,
    XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_VOTE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_REVOTE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_CHECK_VOTE_STATUS,
    XMSG_NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE,
    XMSG_NODES_TO_NODES_DATABASE_SYNC_REQ,
    XMSG_NODES_TO_NODES_DATABASE_SYNC_DATA,
    XMSG_XCASHD_TO_DPOPS_VERIFY,
    XMSG_DPOPS_TO_XCASHD_VERIFY,
    XMSG_SEED_TO_NODES_UPDATE_VOTE_COUNT,
    XMSG_SEED_TO_NODES_PAYOUT,
    XMSG_MESSAGES_COUNT,
    XMSG_NONE = XMSG_MESSAGES_COUNT
} xcash_msg_t;

typedef enum {
    LIMIT_REMOVE = 0,  // Remove from limiter list
    LIMIT_CHECK = 1    // Enforce limit (check & add)
} limit_action_t;

typedef struct {
  char     a[XCASH_WALLET_LENGTH + 1]; // voter wallet address
  uint64_t v;                          // vote total (atomic)
} payout_output_t;

typedef enum {
  DNSSEC_ERR = -1,
  DNSSEC_UNSIGNED = 0,  // no DNSSEC validation (unsigned path)
  DNSSEC_SECURE = 1,    // validated OK
  DNSSEC_BOGUS = 2      // validation attempted but failed
} dnssec_status_t;

typedef struct {
  struct ub_ctx* ctx;
} dnssec_ctx_t;

#endif