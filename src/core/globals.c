#include "globals.h"

// set globals defined in globals.h
int log_level = 0;
int delegate_db_hash_mismatch = 0;
bool is_seed_node = false;
int network_data_nodes_amount = 0;
delegates_t delegates_all[BLOCK_VERIFIERS_TOTAL_AMOUNT] = {0};

char xcash_wallet_public_address[XCASH_PUBLIC_ADDR_LENGTH + 1] = {0};
char current_block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
char previous_block_hash[BLOCK_HASH_LENGTH + 1] = {0};


unsigned char secret_key_data[crypto_vrf_SECRETKEYBYTES] = {0};
char secret_key[VRF_SECRET_KEY_LENGTH +1] = {0};


char vrf_public_key[VRF_PUBLIC_KEY_LENGTH + 1] = {0};

char current_round_part[3] = "1";
char delegates_hash[MD5_HASH_SIZE + 1] = {0};

char delegates_error_list[(MAXIMUM_BUFFER_SIZE_DELEGATES_NAME * 100) + 5000];     // not sure if this is used    
struct current_block_verifiers_majority_vote current_block_verifiers_majority_vote;

mongoc_client_pool_t* database_client_thread_pool = NULL;

pthread_rwlock_t rwlock;
pthread_rwlock_t rwlock_reserve_proofs;
pthread_mutex_t lock;
pthread_mutex_t database_lock;
pthread_mutex_t verify_network_block_lock;
pthread_mutex_t majority_vote_lock;
pthread_mutex_t add_reserve_proof_lock;
pthread_mutex_t invalid_reserve_proof_lock;
pthread_mutex_t database_data_IP_address_lock;
pthread_mutex_t update_current_block_height_lock;
pthread_mutex_t hash_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t majority_vrf_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t delegates_mutex = PTHREAD_MUTEX_INITIALIZER;





atomic_bool server_running = true;

atomic_bool wait_for_vrf_init = true;
atomic_bool wait_for_block_height_init = true;

pthread_t server_thread;




//const char* collection_names[XCASH_DB_COUNT] = {"delegates", "statistics", "reserve_proofs", "reserve_bytes"};
const char* collection_names[XCASH_DB_COUNT] = {"delegates", "statistics"};
bool cleanup_db_before_upsert = false;  // delete db before put content. make sure we have exact copy during initial db syncing
block_verifiers_list_t current_block_verifiers_list;

NetworkNode network_nodes[] = {
    {"XCA1T1uxPiS8oprWpaCrUiiFcQB3KEriiUVqeeqnVtiKakSZmrZhoXKGbzqn4wj3EXY4JFPdJHqGr7iRHVxF4yyE28NvzLQgZf", "seeds.xcashseeds.us",
      "d6d46ef68fb24e13a307bce08e3b31ecdd6601776f5e136bf1be7f5dcfff45c7",0},
    {"XCA1b6Sg5QVBX4jrctQ9SVUcHFqpaGST6bqtFpyoQadTX8SaDs92xR8iec3VfaXKzhYijFiMfwoM4TuYRgy6NXzn5titJnWbra", "seeds.xcashseeds.uk",
      "63232aa1b020a772945bf50ce96db9a04242583118b5a43952f0aaf9ecf7cfbb",0},
    {"XCA1YfTaE1EUJ2cn63ifPjjFYNvoJ4rhUAamj9qqPzp19zd5qwSmWBtPLsop5StXRsZ6bshYp6pcG5BPPrfLfN3q4ALapSU2fu", "seeds.xcashseeds.cc",
      "0abbaa6644e747447f71bb024d6df74c98f53b2bb9f5361e4638673b3a3479c2",0},
    {"XCA1aQciNagSNaMftRCShnMMkQRH4vDN9LiH7VurtS1pWwmPcWkeKEX8anGQkaUnceWBJKiEmYCZZEtrYYAd1GMLAPF11FS6Nu", "seeds.xcashseeds.me",
      "e735f2dea3a1894936088c0423e565634deb7b0cf74412debc5dbc36766dfeaf",0},
    // Sentinel value (empty entry to mark the end)
    {NULL, NULL, NULL, 0}};

char* server_limit_IP_address_list;
char* server_limit_public_address_list;


const char* xcash_net_messages[] = {
    "XCASH_GET_SYNC_INFO",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA",
    "NODES_TO_NODES_VOTE_MAJORITY_RESULTS",
    "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST",
    "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE",
    "NODE_TO_NETWORK_DATA_NODES_CHECK_VOTE_STATUS",
    "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE",
    "NODES_TO_BLOCK_VERIFIERS_RECOVER_DELEGATE",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_REQ",
    "MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_CREATE_NEW_BLOCK"};

// initialize the global variables
void init_globals(void) {
  pthread_rwlock_init(&rwlock, NULL);
  pthread_rwlock_init(&rwlock_reserve_proofs, NULL);
  pthread_mutex_init(&lock, NULL);
  pthread_mutex_init(&database_lock, NULL);
  pthread_mutex_init(&verify_network_block_lock, NULL);
  pthread_mutex_init(&majority_vote_lock, NULL);
  pthread_mutex_init(&add_reserve_proof_lock, NULL);
  pthread_mutex_init(&invalid_reserve_proof_lock, NULL);
  pthread_mutex_init(&database_data_IP_address_lock, NULL);
  pthread_mutex_init(&update_current_block_height_lock, NULL);
  char data[SMALL_BUFFER_SIZE];
  size_t count = 0;
  srand(time(NULL));
  memset(delegates_all, 0, sizeof(delegates_all));
   memset(data,0,sizeof(data));
  memset(current_block_height,0,sizeof(current_block_height));
  server_limit_IP_address_list = (char*)calloc(15728640,sizeof(char)); // 15 MB
  server_limit_public_address_list = (char*)calloc(15728640,sizeof(char)); // 15 MB
   
  // check if the memory needed was allocated on the heap successfully
  if (server_limit_IP_address_list == NULL || server_limit_public_address_list == NULL)
  {
    FATAL_ERROR_EXIT("Can't allocate memory");
  }

  for (count = 0; count < BLOCK_VERIFIERS_TOTAL_AMOUNT; count++)
  {
    memset(current_block_verifiers_list.block_verifiers_name[count],0,sizeof(current_block_verifiers_list.block_verifiers_name[count]));
    memset(current_block_verifiers_list.block_verifiers_public_address[count],0,sizeof(current_block_verifiers_list.block_verifiers_public_address[count]));
    memset(current_block_verifiers_list.block_verifiers_public_key[count],0,sizeof(current_block_verifiers_list.block_verifiers_public_key[count]));
    memset(current_block_verifiers_list.block_verifiers_IP_address[count],0,sizeof(current_block_verifiers_list.block_verifiers_IP_address[count]));
  }

  return;
}