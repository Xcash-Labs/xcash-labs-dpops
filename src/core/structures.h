#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stddef.h>

typedef struct {
    char *block_verifiers_secret_key;
    int total_threads;
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
    char total_vote_count[100];
    char IP_address[256];
    char delegate_name[MAXIMUM_BUFFER_SIZE_DELEGATES_NAME+1];
    char about[1025];
    char website[256];
    char team[256];
    char shared_delegate_status[10];
    char delegate_fee[11];
    char server_specs[1025];
    char block_verifier_score[10];
    char online_status[11];
    char block_verifier_total_rounds[10];
    char block_verifier_online_total_rounds[10];
    char block_verifier_online_percentage[10];
    char block_producer_total_rounds[10];
    char block_producer_block_heights[BUFFER_SIZE_BLOCK_HEIGHTS_DATA];
    char public_key[VRF_PUBLIC_KEY_LENGTH+1];
    char online_status_ck[11];
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

// ---- why is this 100 and not 50
typedef struct {
    char block_verifiers_name[BLOCK_VERIFIERS_TOTAL_AMOUNT][MAXIMUM_BUFFER_SIZE_DELEGATES_NAME+1]; // The block verifiers name
    char block_verifiers_public_address[BLOCK_VERIFIERS_TOTAL_AMOUNT][XCASH_WALLET_LENGTH+1]; // The block verifiers public address
    char block_verifiers_public_key[BLOCK_VERIFIERS_TOTAL_AMOUNT][VRF_PUBLIC_KEY_LENGTH+1]; // The block verifiers public key
    char block_verifiers_IP_address[BLOCK_VERIFIERS_TOTAL_AMOUNT][BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1]; // The block verifiers IP address
    char block_verifiers_vrf_public_key_hex[BLOCK_VERIFIERS_TOTAL_AMOUNT][VRF_PUBLIC_KEY_LENGTH + 1];
    char block_verifiers_random_hex[BLOCK_VERIFIERS_TOTAL_AMOUNT][VRF_RANDOMBYTES_LENGTH * 2 + 1];
    char block_verifiers_vrf_proof_hex[BLOCK_VERIFIERS_TOTAL_AMOUNT][VRF_PROOF_LENGTH + 1];
    char block_verifiers_vrf_beta_hex[BLOCK_VERIFIERS_TOTAL_AMOUNT][VRF_BETA_LENGTH + 1];
    char block_verifier_delegates_db_hash[BLOCK_VERIFIERS_TOTAL_AMOUNT][MD5_HASH_SIZE + 1];
} block_verifiers_list_t;


// no longer used moved above
struct VRF_data {
    char block_verifiers_public_address[BLOCK_VERIFIERS_TOTAL_AMOUNT][XCASH_WALLET_LENGTH + 1];
    char block_verifiers_vrf_public_key_hex[BLOCK_VERIFIERS_TOTAL_AMOUNT][VRF_PUBLIC_KEY_LENGTH + 1];
    char block_verifiers_random_hex[BLOCK_VERIFIERS_TOTAL_AMOUNT][VRF_RANDOMBYTES_LENGTH * 2 + 1];
    char block_verifiers_vrf_proof_hex[BLOCK_VERIFIERS_TOTAL_AMOUNT][VRF_PROOF_LENGTH + 1];
    char block_verifiers_vrf_beta_hex[BLOCK_VERIFIERS_TOTAL_AMOUNT][VRF_BETA_LENGTH + 1];
};

struct blockchain_reserve_bytes {
    char* block_producer_delegates_name_data; // The block_producer_delegates_name
    char* block_producer_delegates_name; // The block_producer_delegates_name_data text
    size_t block_producer_delegates_name_data_length; // The amount of characters of the block_producer_delegates_name_data
    char* block_producer_public_address_data; // The block_producer_public_address
    char* block_producer_public_address; // The block_producer_public_address_data text
    size_t block_producer_public_address_data_length; // The amount of characters of the block_producer_public_address_data
    char* block_producer_node_backup_count_data; // The block_producer_node_backup_count
    char* block_producer_node_backup_count; // The block_producer_node_backup_count_data text
    size_t block_producer_node_backup_count_data_length; // The amount of characters of the block_producer_node_backup_count_data
    char* block_producer_backup_nodes_names_data; // The block_producer_backup_nodes_names
    char* block_producer_backup_nodes_names; // The block_producer_backup_nodes_names_data text
    size_t block_producer_backup_nodes_names_data_length; // The amount of characters of the block_producer_backup_nodes_names_data 
    char* vrf_secret_key_data; // The VRF secret key text for round part 3
    unsigned char* vrf_secret_key; // The VRF secret key for round part 3
    size_t vrf_secret_key_data_length; // The length of the VRF public key for round part 3   
    char* vrf_public_key_data; // The VRF public key text for round part 3
    unsigned char* vrf_public_key; // The VRF public key for round part 3
    size_t vrf_public_key_data_length; // The length of the VRF public key for round part 3
    char* vrf_alpha_string_data; // The VRF alpha string (input string) text for round part 3
    unsigned char* vrf_alpha_string; // The VRF alpha string (input string) for round part 3
    size_t vrf_alpha_string_data_length; // The length of the VRF alpha string (input string) for round part 3
    char* vrf_proof_data; // The VRF proof text for round part 3
    unsigned char* vrf_proof; // The VRF proof for round part 3
    size_t vrf_proof_data_length; // The length of the VRF proof for round part 3
    char* vrf_beta_string_data; // The VRF beta string (output string) text for round part 3
    unsigned char* vrf_beta_string; // The VRF beta string (output string) for round part 3
    size_t vrf_beta_string_data_length; // The length of the VRF beta string (output string) for round part 3
    char* vrf_data_round; // The VRF data for round part 3  
    char* vrf_data; // The VRF data for all of the round parts
    size_t vrf_data_length; // The length of the VRF data for all of the round parts
    char* block_verifiers_vrf_secret_key_data[BLOCK_VERIFIERS_TOTAL_AMOUNT]; // The VRF secret key text that all of the block verifiers create
    unsigned char* block_verifiers_vrf_secret_key[BLOCK_VERIFIERS_TOTAL_AMOUNT]; // The VRF secret key that all of the block verifiers create
    size_t block_verifiers_vrf_secret_key_data_length; // The amount of characters of the block_verifiers_vrf_secret_key_data
    char* block_verifiers_vrf_public_key_data[BLOCK_VERIFIERS_TOTAL_AMOUNT]; // The VRF public key text that all of the block verifiers create
    unsigned char* block_verifiers_vrf_public_key[BLOCK_VERIFIERS_TOTAL_AMOUNT]; // The VRF public key that all of the block verifiers create
    size_t block_verifiers_vrf_public_key_data_length; // The amount of characters of the block_verifiers_vrf_public_key_data
    char* block_verifiers_random_data[BLOCK_VERIFIERS_TOTAL_AMOUNT]; // The random data that all of the block verifiers create
    char* block_verifiers_random_data_text[BLOCK_VERIFIERS_TOTAL_AMOUNT]; // The random data text that all of the block verifiers create
    size_t block_verifiers_random_data_length; // The amount of characters of the block_verifiers_random_data
    char* next_block_verifiers_public_address_data[BLOCK_VERIFIERS_TOTAL_AMOUNT]; // The next_block_verifiers_public_address
    char* next_block_verifiers_public_address[BLOCK_VERIFIERS_TOTAL_AMOUNT]; // The next_block_verifiers_public_address_data text
    size_t next_block_verifiers_public_address_data_length; // The amount of characters of the next_block_verifiers_public_address_data
    char* previous_block_hash_data; // The previous_block_hash
    size_t previous_block_hash_data_length; // The amount of characters of the previous_block_hash_data
    char* block_validation_node_signature_data[BLOCK_VERIFIERS_TOTAL_AMOUNT]; // The block_validation_node_signature
    char* block_validation_node_signature[BLOCK_VERIFIERS_TOTAL_AMOUNT]; // The block_validation_node_signature_data text
    size_t block_validation_node_signature_data_length; // The amount of characters of the block_validation_node_signature_data
};

struct blockchain_data {
    char* network_version_data; // The network_version
    size_t network_version_data_length; // The amount of characters of the network_version_data
    char* timestamp_data; // The timestamp
    size_t timestamp; // Variant decoded timestamp
    size_t timestamp_data_length; // The amount of characters of the timestamp_data
    char* previous_block_hash_data; // The previous_block_hash
    size_t previous_block_hash_data_length; // The amount of characters of the previous_block_hash_data
    char* nonce_data; // The nonce
    size_t nonce_data_length; // The amount of characters of the nonce_data
    char* block_reward_transaction_version_data; // The block_reward_transaction_version
    size_t block_reward_transaction_version_data_length; // The amount of characters of the block_reward_transaction_version_data
    char* unlock_block_data; // The unlock_block
    size_t unlock_block; // Variant decoded unlock_block
    size_t unlock_block_data_length; // The amount of characters of the unlock_block_data
    char* block_reward_input_data; // The block_reward_input
    size_t block_reward_input_data_length; // The amount of characters of the block_reward_input_data
    char* vin_type_data; // The vin_type
    size_t vin_type_data_length; // The amount of characters of the vin_type_data
    char* block_height_data; // The block_height
    size_t block_height; // Variant decoded block_height
    size_t block_height_data_length; // The amount of characters of the block_height_data
    char* block_reward_output_data; // The block_reward_output
    size_t block_reward_output_data_length; // The amount of characters of the block_reward_output_data
    char* block_reward_data; // The block_reward
    size_t block_reward; // Variant decoded block_reward
    size_t block_reward_data_length; // The amount of characters of the block_reward_data
    char* stealth_address_output_tag_data; // The stealth_address_output_tag
    size_t stealth_address_output_tag_data_length; // The amount of characters of the stealth_address_output_tag_data
    char* stealth_address_output_data; // The stealth_address_output
    size_t stealth_address_output_data_length; // The amount of characters of the stealth_address_output_data
    char* extra_bytes_size_data; // The extra_bytes_size
    size_t extra_bytes_size; // Variant decoded extra_bytes_size
    size_t extra_bytes_size_data_length; // The amount of characters of the extra_bytes_size
    char* transaction_public_key_tag_data; // The transaction_public_key_tag
    size_t transaction_public_key_tag_data_length; // The amount of characters of the transaction_public_key_tag
    char* transaction_public_key_data; // The transaction_public_key
    size_t transaction_public_key_data_length; // The amount of characters of the transaction_public_key
    char* extra_nonce_tag_data; // The extra_nonce_tag
    size_t extra_nonce_tag_data_length; // The amount of characters of the extra_nonce_tag
    char* reserve_bytes_size_data; // The reserve_bytes_size
    size_t reserve_bytes_size; // Variant decoded reserve_bytes_size
    size_t reserve_bytes_size_data_length; // The amount of characters of the reserve_bytes_size
    struct blockchain_reserve_bytes blockchain_reserve_bytes; // A blockchain_reserve_bytes struct that holds all of the reserve bytes
    char* ringct_version_data; // The ringct_version
    size_t ringct_version_data_length; // The amount of characters of the ringct_version
    char* transaction_amount_data; // The transaction_amount
    size_t transaction_amount; // Variant decoded transaction_amount
    size_t transaction_amount_data_length; // The amount of characters of the transaction_amount
    char* transactions[MAXIMUM_TRANSACATIONS_PER_BLOCK]; // All of the transactions in the block.
};

struct current_round_part_vote_data {
    char current_vote_results[DATA_HASH_LENGTH+1]; // The SHA2-512 hash of the vote results that the block verifier calculated
    int vote_results_valid; // The current part of the round vote count for a valid vote.
    int vote_results_invalid; // The current part of the round vote count for a invalid vote.
};

struct current_block_verifiers_majority_vote {
    char data[BLOCK_VERIFIERS_AMOUNT][BLOCK_VERIFIERS_AMOUNT][500]; // The data for each received data from each block verifier
};

typedef enum {
    XMSG_NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF,
    XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE,
    XMSG_NODE_TO_NETWORK_DATA_NODES_CHECK_VOTE_STATUS,
    XMSG_NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_RECOVER_DELEGATE,
    XMSG_NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH,
    XMSG_BLOCK_VERIFIERS_TO_NODES_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD,
    XMSG_GET_CURRENT_BLOCK_HEIGHT,
    XMSG_SEND_CURRENT_BLOCK_HEIGHT,
    XMSG_MAIN_NODES_TO_NODES_PART_4_OF_ROUND_CREATE_NEW_BLOCK,
    XMSG_MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_START_BLOCK,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA,
    XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS,
    XMSG_NODES_TO_NODES_VOTE_RESULTS,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_INVALID_RESERVE_PROOFS,
    XMSG_NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST,
    XMSG_NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST,
    XMSG_NETWORK_DATA_NODE_TO_NODE_SEND_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST,
    XMSG_NETWORK_DATA_NODE_TO_NODE_SEND_CURRENT_BLOCK_VERIFIERS_LIST,
    XMSG_BLOCK_VERIFIERS_TO_NETWORK_DATA_NODE_BLOCK_VERIFIERS_CURRENT_TIME,
    XMSG_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_BLOCK_VERIFIERS_CURRENT_TIME,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_ONLINE_STATUS,
    XMSG_NODE_TO_BLOCK_VERIFIERS_CHECK_IF_CURRENT_BLOCK_VERIFIER,
    XMSG_BLOCK_VERIFIERS_TO_NODE_SEND_RESERVE_BYTES,
    XMSG_NETWORK_DATA_NODES_TO_NETWORK_DATA_NODES_DATABASE_SYNC_CHECK,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_DOWNLOAD,      // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_DOWNLOAD,  // server answer
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_DOWNLOAD,  // server answer
    XMSG_XCASH_GET_SYNC_INFO, // get server hashes and block_height
    XMSG_XCASH_GET_BLOCK_PRODUCERS,
    XMSG_XCASH_GET_BLOCK_HASH, 
    XMSG_NODES_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE, 
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_UPDATE,
    XMSG_MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_CREATE_NEW_BLOCK,
    XMSG_MESSAGES_COUNT,
    XMSG_NONE = XMSG_MESSAGES_COUNT
} xcash_msg_t;

#endif