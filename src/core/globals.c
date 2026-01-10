#include "globals.h"

// set globals defined in globals.h

mongoc_client_pool_t* database_client_thread_pool = NULL;
pthread_t server_thread;
dnssec_ctx_t* g_ctx = NULL;
int log_level = 3;  // default level is error + warning + info - change back to 2 once system stabilizes
bool blockchain_ready = false;
int delegate_db_hash_mismatch = 0;
double delegate_fee_percent = 5.0;
uint64_t minimum_payout = 5000;
bool startup_complete = false;
bool is_seed_node = false;
int network_data_nodes_amount = 0;
delegates_t delegates_all[BLOCK_VERIFIERS_TOTAL_AMOUNT] = {0};
delegates_timer_t delegates_timer_all[BLOCK_VERIFIERS_TOTAL_AMOUNT] = {0};
char xcash_wallet_public_address[XCASH_WALLET_LENGTH + 1] = {0};
char current_block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
char previous_block_hash[BLOCK_HASH_LENGTH + 1] = {0};
char sync_token[SYNC_TOKEN_LEN + 1] = {0};
unsigned char secret_key_data[crypto_vrf_SECRETKEYBYTES] = {0};
char secret_key[VRF_SECRET_KEY_LENGTH +1] = {0};
char vrf_public_key[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
char current_round_part[3] = "1";
char delegates_hash[SHA256_HASH_SIZE + 1] = {0};

pthread_mutex_t delegates_all_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t current_block_verifiers_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t producer_refs_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t database_data_IP_address_lock = PTHREAD_MUTEX_INITIALIZER;
atomic_bool server_running             = ATOMIC_VAR_INIT(true);
atomic_bool wait_for_vrf_init          = ATOMIC_VAR_INIT(true);
atomic_bool wait_for_block_height_init = ATOMIC_VAR_INIT(true);
atomic_bool wait_for_consensus_vote    = ATOMIC_VAR_INIT(true);
atomic_bool wait_for_vrf_message       = ATOMIC_VAR_INIT(true);
atomic_bool shutdown_requested         = ATOMIC_VAR_INIT(false);
atomic_bool payment_inprocess          = ATOMIC_VAR_INIT(false);

block_verifiers_list_t current_block_verifiers_list;
NetworkNode network_nodes[] = {
    {"XCS1bZpdMPEjQAksAaYR3n8RMJUEEQ71ndnTRve7F4Uq48CrEJRdNeVXCDJLiE9hRWEgT3W1u8svgQVp6xdeWGBE8gCFNSUzn1", "seeds.xcashseeds.us",
      "cc0f300acb3ee768afabad37763bc485b068fab6bee17faad35b8063c3d32b49",0},
    {"XCS1PyhwyDESVL6Ea7ogyqAN445vTvMAGMKEVMVLWHTwPkeSXc2xRfh5RTyE8zKd38cREh97oXNCtdWBQZCt27K15cF3Ck8f8C", "seeds.xcashseeds.uk",
      "6b4edce4e80d5bd292428403a5831e11263469db0e7dc7de6cae8302ce337b81",0},
    {"XCS1ZKxdiZpU2nhD93nDsvefkeewmxqqUiNzxczvCAw9TxkAyzUNth3Eu2xfgND8qBcTgUEkj8nCeFaGMq8vCePS58w5DaUR13", "seeds.xcashseeds.cc",
      "b13c7ad374b607d5134fa40f9a88235c22db371afce0af1a99733d22133b24d1",0},
    {"XCS1cv4KHS9C83BLWR9hJ37ba8b51wDqb7fnPmuBP7afPyo957k5wgQKtAzGni7QSGbo9DKdyDQXBcW1j8zyCiZxARsBus5t5E", "seeds.xcashseeds.me",
      "035b3f7ff832a64ecf7cfa96ac0e7f8b5dcc203e7fd76ceb592206a1547f2065",0},
    // Sentinel value (empty entry to mark the end)
    {NULL, NULL, NULL, 0}};
const char* endpoints[] = {"updpops.xcashpulse.cc", "updpops.xcashpulse.uk", NULL};
char self_sha[SHA256_DIGEST_SIZE + 1] = {0};
char* server_limit_IP_address_list;
char* server_limit_public_address_list;

const char* xcash_net_messages[] = {
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA",
    "NODES_TO_NODES_VOTE_MAJORITY_RESULTS",
    "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST",
    "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE",
    "NODES_TO_BLOCK_VERIFIERS_VOTE",
    "NODES_TO_BLOCK_VERIFIERS_REVOTE",
    "NODES_TO_BLOCK_VERIFIERS_CHECK_VOTE_STATUS",
    "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE",
    "NODES_TO_NODES_DATABASE_SYNC_REQ",
    "NODES_TO_NODES_DATABASE_SYNC_DATA",
    "XCASHD_TO_DPOPS_VERIFY",
    "DPOPS_TO_XCASHD_VERIFY",
    "SEED_TO_NODES_UPDATE_VOTE_COUNT",
    "SEED_TO_NODES_PAYOUT",
    "SEED_TO_NODES_BANNED"};

// initialize the global variables
void init_globals(void) {
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