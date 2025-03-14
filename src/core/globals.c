#include "globals.h"

  // set globals defined in globals.h
  int log_level = 0;
  bool is_seed_node = false;
  int network_data_nodes_amount = 0;
  delegates_t delegates_all[BLOCK_VERIFIERS_TOTAL_AMOUNT] = {0};
  char xcash_wallet_public_address[XCASH_PUBLIC_ADDR_LENGTH + 1] = {0};
  char current_block_height[BUFFER_SIZE_NETWORK_BLOCK_DATA] = {0};
  char previous_block_hash[BLOCK_HASH_LENGTH + 1] = {0};
  unsigned char secret_key_data[crypto_vrf_SECRETKEYBYTES + 1] = {0};
  char secret_key[VRF_SECRET_KEY_LENGTH + 1] = {0};
  char current_round_part[2] = "1";
  char current_round_part_backup_node[2] = "0";
  int main_network_data_node_create_block = 0;
  mongoc_client_pool_t* database_client_thread_pool = NULL;

  pthread_rwlock_t rwlock;
  pthread_rwlock_t rwlock_reserve_proofs;
  pthread_mutex_t lock;
  pthread_mutex_t database_lock;
  pthread_mutex_t verify_network_block_lock;
  pthread_mutex_t vote_lock;
  pthread_mutex_t add_reserve_proof_lock;
  pthread_mutex_t invalid_reserve_proof_lock;
  pthread_mutex_t database_data_IP_address_lock;
  pthread_mutex_t update_current_block_height_lock;
  pthread_mutex_t hash_mutex = PTHREAD_MUTEX_INITIALIZER;

  const char* collection_names[XCASH_DB_COUNT] = {"delegates", "statistics", "reserve_proofs", "reserve_bytes"};
  bool cleanup_db_before_upsert = false;  // delete db before put content. make sure we have exact copy during initial db syncing

  struct main_nodes_list main_nodes_list = {0};
  block_verifiers_list_t previous_block_verifiers_list;
  block_verifiers_list_t current_block_verifiers_list;
  block_verifiers_list_t next_block_verifiers_list;

  const NetworkNode network_nodes[] = {
    {"XCA1dd7JaWhiuBavUM2ZTJG3GdgPkT1Yd5Q6VvNvnxbEfb6JhUhziTF6w5mMPVeoSv3aa1zGyhedpaa2QQtGEjBo7N6av9nhaU", "xcashseeds.us", 
      "f681a933620c8e9e029d9ac0977d3a2f1d6a64cc49304e079458e3b5d2d4a66f" , 1},
    {"XCA1b6Sg5QVBX4jrctQ9SVUcHFqpaGST6bqtFpyoQadTX8SaDs92xR8iec3VfaXKzhYijFiMfwoM4TuYRgy6NXzn5titJnWbra", "xcashseeds.uk",
       "63232aa1b020a772945bf50ce96db9a04242583118b5a43952f0aaf9ecf7cfbb", 1},
    // Sentinel value (empty entry to mark the end)
    {NULL, NULL, NULL, 0}};

void init_globals(void) {
    pthread_rwlock_init(&rwlock,NULL);
    pthread_rwlock_init(&rwlock_reserve_proofs,NULL);
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&database_lock, NULL);
    pthread_mutex_init(&verify_network_block_lock, NULL);
    pthread_mutex_init(&vote_lock, NULL);
    pthread_mutex_init(&add_reserve_proof_lock, NULL);
    pthread_mutex_init(&invalid_reserve_proof_lock, NULL);
    pthread_mutex_init(&database_data_IP_address_lock, NULL);
    pthread_mutex_init(&update_current_block_height_lock, NULL);
  
    memset(&previous_block_verifiers_list, 0, sizeof(previous_block_verifiers_list));
    memset(&current_block_verifiers_list, 0, sizeof(current_block_verifiers_list));
    memset(&next_block_verifiers_list, 0, sizeof(next_block_verifiers_list));
}