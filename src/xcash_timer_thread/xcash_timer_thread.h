#ifndef XCASH_TIMER_THREAD_H
#define XCASH_TIMER_THREAD_H

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <errno.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "structures.h"
#include "db_functions.h"
#include "xcash_net.h"
#include "network_wallet_functions.h"
#include "network_security_functions.h"
#include "block_verifiers_functions.h"
#include "block_verifiers_synchronize_server_functions.h"

// ---- jobs ----
typedef enum { JOB_PROOF, JOB_ACTIVITY_CK, JOB_IMAGE_CK } job_kind_t;

typedef struct {
  int hour;  // 0..23 local time
  int min;   // 0..59
  job_kind_t kind;
} sched_slot_t;

// 2:00 → IMAGE_CK; 3:00 → ACTIVITY_CK; 4:00 AM & 4:00 PM → PROOF
static const sched_slot_t SLOTS[] = {
  {2,  0, JOB_IMAGE_CK},
  {3,  0, JOB_ACTIVITY_CK},
  {4,  0, JOB_PROOF},
  {16, 0, JOB_PROOF},
};
static const size_t NSLOTS = sizeof(SLOTS)/sizeof(SLOTS[0]);

typedef struct {
  char   *buf;
  size_t  len;
  size_t  cap;
} sbuf_t;

typedef struct {
  char           delegate[XCASH_WALLET_LENGTH + 1]; // delegate address (key)
  payout_output_t *outs;                            // dynamic array of outputs
  size_t         count;                             // used entries
  size_t         cap;                               // allocated entries
} payout_bucket_t;

void* timer_thread(void* arg);
static void run_proof_check(sched_ctx_t* ctx)

#endif