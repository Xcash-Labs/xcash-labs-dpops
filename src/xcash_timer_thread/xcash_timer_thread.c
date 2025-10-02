#include "xcash_timer_thread.h"

#define SCHED_TEST_EVERY_MIN   10
#define SCHED_TEST_MODE        1

// ---- helpers ----
static void lower_thread_priority_batch(void) {
  // Best-effort: reduce CPU weight via niceness (works even if sched change fails)
  if (setpriority(PRIO_PROCESS, 0, 10) == -1) {
    WARNING_PRINT("setpriority failed: %s", strerror(errno));
  }
  struct sched_param sp;
  memset(&sp, 0, sizeof sp);  // priority must be 0 for BATCH
  if (sched_setscheduler(0, SCHED_BATCH, &sp) == -1) {
    WARNING_PRINT("sched_setscheduler(SCHED_BATCH) failed: %s", strerror(errno));
  }
}

static time_t mk_local_next(int hour, int minute, time_t now) {
  struct tm lt;
  localtime_r(&now, &lt);
  lt.tm_sec = 0;
  lt.tm_min = minute;
  lt.tm_hour = hour;
  time_t t = mktime(&lt);  // today at HH:MM
  if (t <= now) {          // already passed → tomorrow
    lt.tm_mday += 1;
    t = mktime(&lt);
  }
  return t;
}

static int pick_next_slot(time_t now, time_t* out_when) {
  int best_idx = -1;
  time_t best_t = 0;
  for (size_t i = 0; i < NSLOTS; ++i) {
    time_t t = mk_local_next(SLOTS[i].hour, SLOTS[i].min, now);
    if (best_idx < 0 || t < best_t) {
      best_idx = (int)i;
      best_t = t;
    }
  }
  if (out_when) *out_when = best_t;
  return best_idx;
}

static void sleep_until(time_t when) {
  for (;;) {
    PRINT_ERROR("TEST...............................");
    if (atomic_load_explicit(&shutdown_requested, memory_order_relaxed)) return;
    time_t now = time(NULL);
    if (now >= when) return;
    time_t d = when - now;
    if (d > 5) d = 5;  // short chunks so we wake quickly on shutdown
    struct timespec ts = {d, 0};
    nanosleep(&ts, NULL);
  }
}

static void add_vote_sum(/*in/out*/ char addrs[][XCASH_WALLET_LENGTH + 1],
                         /*in/out*/ int64_t totals[],
                         /*in/out*/ size_t* pcount,
                         const char* addr, int64_t amt) {
  // Try to find existing bucket
  for (size_t i = 0; i < *pcount; ++i) {
    if (strcmp(addrs[i], addr) == 0) {
      totals[i] += amt;
      return;
    }
  }
  // New bucket
  if (*pcount >= BLOCK_VERIFIERS_TOTAL_AMOUNT) {
    WARNING_PRINT("vote_sums full; dropping contribution for %.12s…", addr);
    return;
  }
  size_t n = strnlen(addr, XCASH_WALLET_LENGTH + 1);
  if (n == 0 || n > XCASH_WALLET_LENGTH) {
    WARNING_PRINT("bad delegate address length=%zu, skipping", n);
    return;
  }
  memcpy(addrs[*pcount], addr, n);
  addrs[*pcount][n] = '\0';
  totals[*pcount] = amt;
  ++(*pcount);
}

static void run_proof_check(sched_ctx_t* ctx) {
  mongoc_client_t* c = mongoc_client_pool_pop(ctx->pool);
  if (!c) {
    ERROR_PRINT("Failed to pop a client from the mongoc_client_pool");
    return;
  }

  mongoc_collection_t* coll =
      mongoc_client_get_collection(c, DATABASE_NAME, DB_COLLECTION_RESERVE_PROOFS);
  if (!coll) {
    ERROR_PRINT("reserve_proofs: get_collection failed");
    mongoc_client_pool_push(ctx->pool, c);
    return;
  }

  // Projection: only fields we need
  bson_t* query = bson_new();  // {}
  bson_t* opts  = BCON_NEW(
      "projection", "{",
        "_id",                      BCON_INT32(1),
        "public_address_voted_for", BCON_INT32(1),
        "total_vote",               BCON_INT32(1),
        "reserve_proof",            BCON_INT32(1),
      "}",
      "noCursorTimeout", BCON_BOOL(true)
  );

  mongoc_cursor_t* cur = mongoc_collection_find_with_opts(coll, query, opts, NULL);
  if (!cur) {
    ERROR_PRINT("reserve_proofs: find_with_opts failed");
    bson_destroy(opts);
    bson_destroy(query);
    mongoc_collection_destroy(coll);
    mongoc_client_pool_push(ctx->pool, c);
    return;
  }

  // --- per-delegate aggregators (small, fixed upper bound) ---
  char    agg_addr[BLOCK_VERIFIERS_TOTAL_AMOUNT][XCASH_WALLET_LENGTH + 1];
  int64_t agg_total[BLOCK_VERIFIERS_TOTAL_AMOUNT];
  memset(agg_addr,  0, sizeof agg_addr);
  memset(agg_total, 0, sizeof agg_total);
  size_t  agg_count = 0;

  const bson_t* doc = NULL;
  bson_error_t cerr;
  size_t seen = 0, invalid = 0, deleted = 0, skipped = 0;

  while (mongoc_cursor_next(cur, &doc)) {
    ++seen;
    if (atomic_load_explicit(&shutdown_requested, memory_order_relaxed)) break;

    bson_iter_t it;
    const char* voter    = NULL;  // _id
    const char* delegate = NULL;  // public_address_voted_for
    const char* proof    = NULL;  // reserve_proof
    int64_t claimed_total = 0;

    if (bson_iter_init_find(&it, doc, "_id") && BSON_ITER_HOLDS_UTF8(&it))
      voter = bson_iter_utf8(&it, NULL);

    if (bson_iter_init_find(&it, doc, "public_address_voted_for") && BSON_ITER_HOLDS_UTF8(&it))
      delegate = bson_iter_utf8(&it, NULL);

    if (bson_iter_init_find(&it, doc, "reserve_proof") && BSON_ITER_HOLDS_UTF8(&it))
      proof = bson_iter_utf8(&it, NULL);

    // total_vote must be an integer
    if (bson_iter_init_find(&it, doc, "total_vote")) {
      if (BSON_ITER_HOLDS_INT64(&it))       claimed_total = bson_iter_int64(&it);
      else if (BSON_ITER_HOLDS_INT32(&it))  claimed_total = (int64_t)bson_iter_int32(&it);
      else {
        ERROR_PRINT("reserve_proofs: total_vote has unexpected type=%d for id=%.12s…",
                    (int)bson_iter_type(&it), voter ? voter : "(unknown)");
        ++skipped;
        continue;
      }
    } else {
      ERROR_PRINT("reserve_proofs: missing total_vote for id=%.12s…",
                  voter ? voter : "(unknown)");
      ++skipped;
      continue;
    }

    if (!voter || !delegate || !proof) {
      ++skipped;
      WARNING_PRINT("reserve_proofs: missing required field(s), skipping one doc");
      continue;
    }

    // Validate the proof against the delegate address & claimed amount
    int rc = check_reserve_proofs((uint64_t)claimed_total, delegate, proof);
    bool ok = (rc == XCASH_OK);

    if (!ok) {
      ++invalid;

      // delete by _id (voter)
      bson_t del_filter; bson_init(&del_filter);
      BSON_APPEND_UTF8(&del_filter, "_id", voter);
      bson_error_t derr;
      if (mongoc_collection_delete_one(coll, &del_filter, NULL, NULL, &derr)) {
        ++deleted;
        INFO_PRINT("Deleted invalid reserve_proof id=%.12s… (delegate=%.12s…)",
                   voter, delegate);
      } else {
        WARNING_PRINT("Failed to delete invalid reserve_proof id=%.12s… : %s",
                      voter, derr.message);
      }
      bson_destroy(&del_filter);
      continue;
    }

    // Valid proof → accumulate into delegate bucket
    add_vote_sum(agg_addr, agg_total, &agg_count, delegate, claimed_total);
  }

  if (mongoc_cursor_error(cur, &cerr)) {
    ERROR_PRINT("reserve_proofs cursor error: %s", cerr.message);
  } else {
    INFO_PRINT("reserve_proofs scan complete: seen=%zu invalid=%zu deleted=%zu skipped=%zu",
               seen, invalid, deleted, skipped);
  }

  mongoc_cursor_destroy(cur);
  bson_destroy(opts);
  bson_destroy(query);
  mongoc_collection_destroy(coll);

  // --- Apply per-delegate totals back to delegates collection ---
  if (agg_count > 0) {
    mongoc_collection_t* dcoll =
        mongoc_client_get_collection(c, DATABASE_NAME, DB_COLLECTION_DELEGATES);
    if (!dcoll) {
      ERROR_PRINT("delegates: get_collection failed; cannot write totals");
    } else {
      for (size_t i = 0; i < agg_count; ++i) {
        bson_t filter; bson_init(&filter);
        BSON_APPEND_UTF8(&filter, "public_address", agg_addr[i]);

        bson_t set, update; bson_init(&set); bson_init(&update);
        BSON_APPEND_INT64(&set, "total_vote_count", (int64_t)agg_total[i]);
        BSON_APPEND_DOCUMENT(&update, "$set", &set);

        bson_error_t uerr;
        if (!mongoc_collection_update_one(dcoll, &filter, &update, NULL, NULL, &uerr)) {
          WARNING_PRINT("delegate total update failed addr=%.12s… : %s",
                        agg_addr[i], uerr.message);
        } else {
          DEBUG_PRINT("delegate total updated addr=%.12s… total=%lld",
                      agg_addr[i], (long long)agg_total[i]);
        }

        bson_destroy(&update);
        bson_destroy(&set);
        bson_destroy(&filter);
      }
      mongoc_collection_destroy(dcoll);
    }
  }

  mongoc_client_pool_push(ctx->pool, c);
}


static void run_payout(sched_ctx_t* ctx) {
  //  mongoc_client_t* c = mongoc_client_pool_pop(ctx->pool);
  //  if (!c) return;
  // TODO: build & send payouts (with confirmations, thresholds, idempotency)
  //  mongoc_client_pool_push(ctx->pool, c);
}

// ---- single scheduler thread ----
void* timer_thread(void* arg) {
  lower_thread_priority_batch();
  sched_ctx_t* ctx = (sched_ctx_t*)arg;

  for (;;) {
    if (atomic_load_explicit(&shutdown_requested, memory_order_relaxed)) break;

    time_t now = time(NULL), run_at;

#ifndef SCHED_TEST_MODE
    // --- normal: pick next slot from SLOTS ---
    int idx = pick_next_slot(now, &run_at);
    if (idx < 0) break;  // shouldn't happen
#else
    // --- test mode: run every N minutes (local time) ---
    run_at = mk_local_next_every_minutes(SCHED_TEST_EVERY_MIN, now);
#endif

    time_t wake = run_at - WAKEUP_SKEW_SEC;
    if (wake < now) wake = now;

    // pre-wake, then align to exact minute
    sleep_until(wake);
    if (atomic_load_explicit(&shutdown_requested, memory_order_relaxed)) break;
    sleep_until(run_at);
    if (atomic_load_explicit(&shutdown_requested, memory_order_relaxed)) break;

#ifndef SCHED_TEST_MODE
    // dispatch based on role
    const sched_slot_t* slot = &SLOTS[idx];

    if (slot->kind == JOB_PROOF) {
      if (is_seed_node) {
        INFO_PRINT("Checking for Primary");
        if (seed_is_primary()) {
          INFO_PRINT("Scheduler: running PROOF CHECK at %02d:%02d", slot->hour, slot->min);
          run_proof_check(ctx);
        }
      }
    } else {
      if (!is_seed_node) {
        INFO_PRINT("Scheduler: running PAYOUT at %02d:%02d", slot->hour, slot->min);
        run_payout(ctx);
      }
    }

#else
    // ---- test dispatch every N minutes ----
    if (is_seed_node()) {
      if (seed_is_primary()) {
        INFO_PRINT("Test scheduler: PROOF CHECK (every %d min)", SCHED_TEST_EVERY_MIN);
        run_proof_check(ctx);
      } else {
        DEBUG_PRINT("Test scheduler: not primary seed — skip proof");
      }
    } else {
      INFO_PRINT("Test scheduler: PAYOUT (every %d min)", SCHED_TEST_EVERY_MIN);
      run_payout(ctx);
    }
#endif


  }
  return NULL;
}