#ifndef STRUCTURES_H
#define STRUCTURES_H

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

#endif