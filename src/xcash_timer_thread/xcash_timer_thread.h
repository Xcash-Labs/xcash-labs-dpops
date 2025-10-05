#ifndef XCASH_TIMER_THREAD_H
#define XCASH_TIMER_THREAD_H

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sodium.h> 
#include <errno.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "structures.h"
#include "db_functions.h"
#include "network_wallet_functions.h"
#include "network_security_functions.h"
#include "xcash_net.h"
#include "block_verifiers_functions.h"
#include "block_verifiers_synchronize_server_functions.h"

// ---- jobs ----
typedef enum { JOB_PROOF, JOB_PAYOUT } job_kind_t;

typedef struct {
  int hour;  // 0..23 local time
  int min;   // 0..59
  job_kind_t kind;
} sched_slot_t;

// 3:00 AM & 3:00 PM → PROOF; 6:00 AM & 6:00 PM → PAYOUT
static const sched_slot_t SLOTS[] = {
  {3,  0, JOB_PROOF},
  {15, 0, JOB_PROOF},
  {6,  0, JOB_PAYOUT},
  {18, 0, JOB_PAYOUT},
};
static const size_t NSLOTS = sizeof(SLOTS)/sizeof(SLOTS[0]);

typedef struct {
  char     a[XCASH_WALLET_LENGTH + 1];  // voter wallet address
  uint64_t v;                           // vote total (atomic)
} payout_output_t;

typedef struct {
  char           delegate[XCASH_WALLET_LENGTH + 1]; // delegate address (key)
  payout_output_t *outs;                            // dynamic array of outputs
  size_t         count;                             // used entries
  size_t         cap;                               // allocated entries
} payout_bucket_t;

void* timer_thread(void* arg);

#endif