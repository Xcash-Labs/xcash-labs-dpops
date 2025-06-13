#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stddef.h>

typedef struct {
    char *block_verifiers_secret_key;
    bool delegates_website;
    bool shared_delegates_website;
    float fee;
    unsigned long long minimum_amount;
    bool init_db_from_seeds;
    bool init_db_from_top;
} arg_config_t;

// Define a struct to store network node data
typedef struct {
    const char *seed_public_address;
    const char *ip_address;
    const char *seed_public_key;
    int online_status;
} NetworkNode;

// xcash-next
typedef struct  {
    char public_address[XCASH_WALLET_LENGTH+1];
    uint64_t total_vote_count;
    char IP_address[IP_LENGTH+1];
    char delegate_name[MAXIMUM_BUFFER_SIZE_DELEGATES_NAME+1];
    char about[1025];
    char website[256];
    char team[256];
    char delegate_type[10];
    float delegate_fee; 
    char server_specs[1025];
    char online_status[11];
    uint64_t block_verifier_total_rounds;
    uint64_t block_verifier_online_total_rounds;
    uint64_t block_producer_total_rounds;
    char public_key[VRF_PUBLIC_KEY_LENGTH+1];
    uint64_t registration_timestamp;
} delegates_t;

// database struct
struct database_document_fields {
    size_t count; // The amount of items in the database_document_fields struct
    char* item[TOTAL_DELEGATES_DATABASE_FIELDS+1]; // The item
    char* value[TOTAL_DELEGATES_DATABASE_FIELDS+1]; // The value
};

struct database_multiple_documents_fields {
    size_t document_count; // The amount of documents in the database_multiple_documents_fields
    size_t database_fields_count; // The amount of items in each document
    char* item[MAXIMUM_DATABASE_COLLECTION_DOCUMENTS][TOTAL_DELEGATES_DATABASE_FIELDS+1]; // The item
    char* value[MAXIMUM_DATABASE_COLLECTION_DOCUMENTS][TOTAL_DELEGATES_DATABASE_FIELDS+1]; // The value
};

typedef struct {
    char block_verifiers_name[BLOCK_VERIFIERS_AMOUNT][MAXIMUM_BUFFER_SIZE_DELEGATES_NAME+1]; // The block verifiers name
    char block_verifiers_public_address[BLOCK_VERIFIERS_AMOUNT][XCASH_WALLET_LENGTH+1]; // The block verifiers public address
    char block_verifiers_public_key[BLOCK_VERIFIERS_AMOUNT][VRF_PUBLIC_KEY_LENGTH+1]; // The block verifiers public key
    char block_verifiers_IP_address[BLOCK_VERIFIERS_AMOUNT][BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1]; // The block verifiers IP address
    char block_verifiers_vrf_public_key_hex[BLOCK_VERIFIERS_AMOUNT][VRF_PUBLIC_KEY_LENGTH + 1];
    char block_verifiers_random_hex[BLOCK_VERIFIERS_AMOUNT][VRF_RANDOMBYTES_LENGTH * 2 + 1];
    char block_verifiers_vrf_proof_hex[BLOCK_VERIFIERS_AMOUNT][VRF_PROOF_LENGTH + 1];
    char block_verifiers_vrf_beta_hex[BLOCK_VERIFIERS_AMOUNT][VRF_BETA_LENGTH + 1];
    int block_verifiers_vote_total[BLOCK_VERIFIERS_AMOUNT];
    uint8_t block_verifiers_voted[BLOCK_VERIFIERS_AMOUNT];
} block_verifiers_list_t;

struct current_block_verifiers_majority_vote {
    char data[BLOCK_VERIFIERS_AMOUNT][BLOCK_VERIFIERS_AMOUNT][500]; // The data for each received data from each block verifier
};

typedef enum {
    XMSG_XCASH_GET_SYNC_INFO,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA,
    XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS,
    XMSG_NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST,
    XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE,
    XMSG_NODE_TO_NETWORK_DATA_NODES_CHECK_VOTE_STATUS,
    XMSG_NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_RECOVER_DELEGATE,
    XMSG_NODES_TO_NODES_DATABASE_SYNC_REQ,
    XMSG_NODES_TO_NODES_DATABASE_SYNC_DATA,
    XMSG_MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_CREATE_NEW_BLOCK,
    XMSG_MESSAGES_COUNT,
    XMSG_NONE = XMSG_MESSAGES_COUNT
} xcash_msg_t;

typedef enum {
    LIMIT_REMOVE = 0,  // Remove from limiter list
    LIMIT_CHECK = 1    // Enforce limit (check & add)
} limit_action_t;

#endif