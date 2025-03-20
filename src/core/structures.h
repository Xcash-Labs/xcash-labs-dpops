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
    char online_status[10];
    char block_verifier_total_rounds[10];
    char block_verifier_online_total_rounds[10];
    char block_verifier_online_percentage[10];
    char block_producer_total_rounds[10];
    char block_producer_block_heights[BUFFER_SIZE_BLOCK_HEIGHTS_DATA];
    char public_key[VRF_PUBLIC_KEY_LENGTH+1];
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

struct main_nodes_list {
    char block_producer_public_address[XCASH_WALLET_LENGTH+1]; // The block producers public address
    char block_producer_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1]; // The block producers IP address
    char block_producer_backup_block_verifier_1_public_address[XCASH_WALLET_LENGTH+1]; // The block producers backup node 1 public address
    char block_producer_backup_block_verifier_1_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1]; // The block producers backup node 1 IP address
    char block_producer_backup_block_verifier_2_public_address[XCASH_WALLET_LENGTH+1]; // The block producers backup node 2 public address
    char block_producer_backup_block_verifier_2_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1]; // The block producers backup node 2 IP address
    char block_producer_backup_block_verifier_3_public_address[XCASH_WALLET_LENGTH+1]; // The block producers backup node 3 public address
    char block_producer_backup_block_verifier_3_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1]; // The block producers backup node 3 IP address
    char block_producer_backup_block_verifier_4_public_address[XCASH_WALLET_LENGTH+1]; // The block producers backup node 4 public address
    char block_producer_backup_block_verifier_4_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1]; // The block producers backup node 4 IP address
    char block_producer_backup_block_verifier_5_public_address[XCASH_WALLET_LENGTH+1]; // The block producers backup node 5 public address
    char block_producer_backup_block_verifier_5_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1]; // The block producers backup node 5 IP address    
};

typedef struct {
    char block_verifiers_name[BLOCK_VERIFIERS_TOTAL_AMOUNT][MAXIMUM_BUFFER_SIZE_DELEGATES_NAME+1]; // The block verifiers name
    char block_verifiers_public_address[BLOCK_VERIFIERS_TOTAL_AMOUNT][XCASH_WALLET_LENGTH+1]; // The block verifiers public address
    char block_verifiers_public_key[BLOCK_VERIFIERS_TOTAL_AMOUNT][VRF_PUBLIC_KEY_LENGTH+1]; // The block verifiers public key
    char block_verifiers_IP_address[BLOCK_VERIFIERS_TOTAL_AMOUNT][BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1]; // The block verifiers IP address
} block_verifiers_list_t;


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