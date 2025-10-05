#include "xcash_timer_thread.h"

#define SCHED_TEST_EVERY_MIN 10
#define SCHED_TEST_MODE 1

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
    if (atomic_load_explicit(&shutdown_requested, memory_order_relaxed)) return;
    time_t now = time(NULL);
    if (now >= when) return;
    time_t d = when - now;
    if (d > 5) d = 5;  // short chunks so we wake quickly on shutdown
    struct timespec ts = {d, 0};
    nanosleep(&ts, NULL);
  }
}

/* Compute SHA-256 over the canonical encoding of an outputs array.
   Canonical bytes per entry:
     - uint16 LE length of address (len16)
     - address bytes (ASCII, exactly 'len16' bytes; no NUL included)
     - uint64 LE amount
   The order of 'outs' MUST be deterministic across nodes.
   out32: receives 32 bytes of the SHA-256 digest.
*/
void outputs_digest_sha256(const payout_output_t *outs, size_t n, uint8_t out32[32]) {
  crypto_hash_sha256_state st;
  crypto_hash_sha256_init(&st);
  for (size_t i = 0; i < n; ++i) {
    uint16_t alen = (uint16_t)strnlen(outs[i].a, XCASH_WALLET_LENGTH);
    uint8_t le16[2] = { (uint8_t)(alen & 0xFF), (uint8_t)((alen >> 8) & 0xFF) };
    crypto_hash_sha256_update(&st, le16, 2);
    crypto_hash_sha256_update(&st, (const unsigned char*)outs[i].a, alen);
    uint8_t le64[8];
    for (int b = 0; b < 8; ++b) le64[b] = (outs[i].v >> (8*b)) & 0xFF;
    crypto_hash_sha256_update(&st, le64, 8);
  }
  crypto_hash_sha256_final(&st, out32);
}

// Helpers to rind or create a bucket by delegate
static int get_bucket_index(payout_bucket_t buckets[],
                            size_t* bucket_count,
                            const char* delegate) {
  for (size_t i = 0; i < *bucket_count; ++i) {
    if (strcmp(buckets[i].delegate, delegate) == 0) return (int)i;
  }
  if (*bucket_count >= BLOCK_VERIFIERS_TOTAL_AMOUNT) {
    return -1;  // too many delegates (shouldn't happen if capped)
  }
  // create new bucket
  payout_bucket_t* b = &buckets[*bucket_count];
  memset(b, 0, sizeof *b);
  strncpy(b->delegate, delegate, XCASH_WALLET_LENGTH);
  b->delegate[XCASH_WALLET_LENGTH] = '\0';
  b->outs = NULL;
  b->count = 0;
  b->cap = 0;
  return (int)(*bucket_count)++;
}

static int bucket_push_output(payout_bucket_t* b,
                              const char* voter_addr,
                              uint64_t amount_atomic) {
  if (b->count == b->cap) {
    size_t new_cap = (b->cap == 0) ? 256 : (b->cap * 2);
    payout_output_t* p = (payout_output_t*)realloc(b->outs, new_cap * sizeof(*p));
    if (!p) return 0;
    b->outs = p;
    b->cap = new_cap;
  }
  payout_output_t* o = &b->outs[b->count++];
  strncpy(o->a, voter_addr, XCASH_WALLET_LENGTH);
  o->a[XCASH_WALLET_LENGTH] = '\0';
  o->v = amount_atomic;
  return 1;
}

static void free_buckets(payout_bucket_t buckets[], size_t bucket_count) {
  for (size_t i = 0; i < bucket_count; ++i) {
    free(buckets[i].outs);
    buckets[i].outs = NULL;
    buckets[i].cap = buckets[i].count = 0;
  }
}

// Small helper to keep per-delegate running totals
static void add_vote_sum(char addrs[][XCASH_WALLET_LENGTH + 1],
                         int64_t totals[],
                         size_t* pcount,
                         const char* addr,
                         int64_t amt) {
  // Find existing bucket
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

static int sbuf_init(sbuf_t* s, size_t cap) {
  s->cap = cap ? cap : 4096;
  s->len = 0;
  s->buf = (char*)malloc(s->cap);
  if (!s->buf) return 0;
  s->buf[0] = '\0';
  return 1;
}

static int sbuf_ensure(sbuf_t* s, size_t need) {
  if (s->len + need + 1 <= s->cap) return 1;
  size_t nc = s->cap * 2;
  while (nc < s->len + need + 1) nc *= 2;
  char* p = (char*)realloc(s->buf, nc);
  if (!p) return 0;
  s->buf = p;
  s->cap = nc;
  return 1;
}

static int sbuf_addf(sbuf_t* s, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  va_list ap2;
  va_copy(ap2, ap);
  int need = vsnprintf(NULL, 0, fmt, ap);  // how many bytes
  va_end(ap);
  if (need < 0 || !sbuf_ensure(s, (size_t)need)) {
    va_end(ap2);
    return 0;
  }
  vsnprintf(s->buf + s->len, s->cap - s->len, fmt, ap2);
  va_end(ap2);
  s->len += (size_t)need;
  return 1;
}

/*---------------------------------------------------------------------------------------------------------
Name: add_vote_sum
Description:
  Accumulates a vote amount into the per-delegate running totals. If the delegate
  already has a bucket, increments it; otherwise creates a new bucket as long as
  the array capacity (BLOCK_VERIFIERS_TOTAL_AMOUNT) is not exceeded.

Parameters:
  addrs   - (IN/OUT) Array of delegate addresses; each slot is a null-terminated string.
  totals  - (IN/OUT) Parallel array of 64-bit running totals for each address.
  pcount  - (IN/OUT) Current number of active buckets; incremented when a new bucket is added.
  addr    - (IN)     Delegate address to accumulate into.
  amt     - (IN)     Amount to add (must be ≥ 0; caller ensures semantics).

Returns:
  void

Notes:
  - If capacity is reached, logs a warning and drops the contribution.
  - Truncates/guards address length to XCASH_WALLET_LENGTH.
---------------------------------------------------------------------------------------------------------*/
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

  // Build query/projection options
  bson_t* query = bson_new();  // {}
  bson_t* opts = BCON_NEW(
      "projection", "{",
      "_id", BCON_INT32(1),
      "public_address_voted_for", BCON_INT32(1),
      "total_vote", BCON_INT32(1),
      "reserve_proof", BCON_INT32(1),
      "}",
      "noCursorTimeout", BCON_BOOL(true));
  if (!query || !opts) {
    ERROR_PRINT("reserve_proofs: OOM building query/options");
    if (opts) bson_destroy(opts);
    if (query) bson_destroy(query);
    mongoc_collection_destroy(coll);
    mongoc_client_pool_push(ctx->pool, c);
    return;
  }

  mongoc_cursor_t* cur = mongoc_collection_find_with_opts(coll, query, opts, NULL);
  if (!cur) {
    ERROR_PRINT("reserve_proofs: find_with_opts failed");
    bson_destroy(opts);
    bson_destroy(query);
    mongoc_collection_destroy(coll);
    mongoc_client_pool_push(ctx->pool, c);
    return;
  }

  // Per-delegate aggregators (bounded by number of delegates)
  char agg_addr[BLOCK_VERIFIERS_TOTAL_AMOUNT][XCASH_WALLET_LENGTH + 1];
  int64_t agg_total[BLOCK_VERIFIERS_TOTAL_AMOUNT];
  memset(agg_addr, 0, sizeof agg_addr);
  memset(agg_total, 0, sizeof agg_total);
  size_t agg_count = 0;

  const bson_t* doc = NULL;
  bson_error_t cerr;
  size_t seen = 0, invalid = 0, deleted = 0, skipped = 0;

  payout_bucket_t pay_buckets[BLOCK_VERIFIERS_TOTAL_AMOUNT];
  memset(pay_buckets, 0, sizeof pay_buckets);
  size_t pay_bucket_count = 0;

  while (mongoc_cursor_next(cur, &doc)) {
    ++seen;
    if (atomic_load_explicit(&shutdown_requested, memory_order_relaxed)) break;

    bson_iter_t it;
    const char* voter = NULL;     // _id (voter public address)
    const char* delegate = NULL;  // public_address_voted_for
    const char* proof = NULL;     // reserve_proof
    int64_t claimed_total = 0;

    if (bson_iter_init_find(&it, doc, "_id") && BSON_ITER_HOLDS_UTF8(&it))
      voter = bson_iter_utf8(&it, NULL);

    if (bson_iter_init_find(&it, doc, "public_address_voted_for") && BSON_ITER_HOLDS_UTF8(&it))
      delegate = bson_iter_utf8(&it, NULL);

    if (bson_iter_init_find(&it, doc, "reserve_proof") && BSON_ITER_HOLDS_UTF8(&it))
      proof = bson_iter_utf8(&it, NULL);

    // total_vote must be integer and positive
    if (bson_iter_init_find(&it, doc, "total_vote")) {
      if (BSON_ITER_HOLDS_INT64(&it))
        claimed_total = bson_iter_int64(&it);
      else if (BSON_ITER_HOLDS_INT32(&it))
        claimed_total = (int64_t)bson_iter_int32(&it);
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

    if (claimed_total <= 0) {
      WARNING_PRINT("reserve_proofs: non-positive total_vote=%lld for id=%.12s… — skipping",
                    (long long)claimed_total, voter ? voter : "(unknown)");
      ++skipped;
      continue;
    }

    if (!voter || !delegate || !proof) {
      ++skipped;
      WARNING_PRINT("reserve_proofs: missing required field(s), skipping one doc");
      continue;
    }

    // Validate the proof against the voter address & claimed amount
    int rc = check_reserve_proofs((uint64_t)claimed_total, voter, proof);
    bool ok = (rc == XCASH_OK);

    if (!ok) {
      ++invalid;

      // delete invalid proof by _id (voter)
      bson_t del_filter;
      bson_init(&del_filter);
      BSON_APPEND_UTF8(&del_filter, "_id", voter);
      bson_error_t derr;
      if (mongoc_collection_delete_one(coll, &del_filter, NULL, NULL, &derr)) {
        ++deleted;
        WARNING_PRINT("Deleted invalid reserve_proof id=%.12s… (delegate=%.12s…)",
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

    // Also accumulate per-voter outputs for this delegate
    int idx = get_bucket_index(pay_buckets, &pay_bucket_count, delegate);
    if (idx < 0) {
      ERROR_PRINT("Too many delegate buckets while collecting outputs; skipping one entry");
    } else {
      if (!bucket_push_output(&pay_buckets[idx], voter, (uint64_t)claimed_total)) {
        ERROR_PRINT("OOM while appending payout output; skipping one entry");
      }
    }
  }

  if (mongoc_cursor_error(cur, &cerr)) {
    ERROR_PRINT("reserve_proofs cursor error: %s", cerr.message);
  } else {
    WARNING_PRINT("reserve_proofs scan complete: seen=%zu invalid=%zu deleted=%zu skipped=%zu",
                  seen, invalid, deleted, skipped);
  }

  bool stop_after_scan = atomic_load_explicit(&shutdown_requested, memory_order_relaxed);

  mongoc_cursor_destroy(cur);
  bson_destroy(opts);
  bson_destroy(query);
  mongoc_collection_destroy(coll);

  // Wait for correct time to load from delegates_all
  sync_block_verifiers_minutes_and_seconds(0, 51);
  char save_block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
  char save_block_hash[BLOCK_HASH_LENGTH + 1] = {0};
  snprintf(save_block_height, sizeof save_block_height, "%" PRIu64, (uint64_t)current_block_height);
  strncpy(save_block_hash, previous_block_hash, sizeof save_block_hash - 1);
  save_block_hash[sizeof save_block_hash - 1] = '\0';
  size_t online_count = 0;
  pthread_mutex_lock(&current_block_verifiers_lock);
  memset(delegates_timer_all, 0, sizeof delegates_timer_all);
  for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; ++i) {
    if (delegates_all[i].public_address[0] == '\0' ||
        delegates_all[i].IP_address[0] == '\0') continue;
    if (strcmp(delegates_all[i].online_status, "true") == 0) {
      strcpy(delegates_timer_all[online_count].public_address, delegates_all[i].public_address);
      strcpy(delegates_timer_all[online_count].IP_address, delegates_all[i].IP_address);
      online_count++;
    }
  }
  pthread_mutex_unlock(&current_block_verifiers_lock);

  // Apply per-delegate totals only if not shutting down
  if (!stop_after_scan && agg_count > 0) {
    mongoc_collection_t* dcoll =
        mongoc_client_get_collection(c, DATABASE_NAME, DB_COLLECTION_DELEGATES);
    if (!dcoll) {
      ERROR_PRINT("delegates: get_collection failed; cannot write totals");
    } else {
      for (size_t i = 0; i < agg_count; ++i) {
        bson_t filter;
        bson_init(&filter);
        BSON_APPEND_UTF8(&filter, "public_address", agg_addr[i]);

        // --- Projection: only fetch total_vote_count, and limit 1
        bson_t opts_ck;
        bson_init(&opts_ck);
        bson_t proj;
        bson_init(&proj);
        BSON_APPEND_INT32(&proj, "total_vote_count", 1);
        BSON_APPEND_DOCUMENT(&opts_ck, "projection", &proj);
        BSON_APPEND_INT64(&opts_ck, "limit", 1);

        // --- Query current value
        int64_t current_total = -1;  // -1 => "missing/unknown"
        bool have_current = false;

        mongoc_cursor_t* cur_ck = mongoc_collection_find_with_opts(dcoll, &filter, &opts_ck, NULL);
        if (!cur_ck) {
          WARNING_PRINT("delegate total read failed addr=%.12s… (cursor init)", agg_addr[i]);
        } else {
          const bson_t* doc_ck;
          if (mongoc_cursor_next(cur_ck, &doc_ck)) {
            bson_iter_t it;
            if (bson_iter_init_find(&it, doc_ck, "total_vote_count") &&
                (BSON_ITER_HOLDS_INT32(&it) || BSON_ITER_HOLDS_INT64(&it))) {
              current_total = bson_iter_as_int64(&it);
              have_current = true;
            }
          }
          bson_error_t ck_err;
          if (mongoc_cursor_error(cur_ck, &ck_err)) {
            WARNING_PRINT("delegate total read failed addr=%.12s… : %s", agg_addr[i], ck_err.message);
          }
          mongoc_cursor_destroy(cur_ck);
        }

        bson_destroy(&proj);
        bson_destroy(&opts_ck);

        // --- Compare and skip update if no change
        int64_t new_total = (int64_t)agg_total[i];
        if (have_current && current_total == new_total) {
          WARNING_PRINT("delegate total unchanged addr=%.12s… total=%lld (skip)",
                        agg_addr[i], (long long)new_total);

          bson_destroy(&filter);
          continue;
        }

        // --- Apply update only when needed
        bson_t set;
        bson_init(&set);
        BSON_APPEND_INT64(&set, "total_vote_count", new_total);

        bson_t update;
        bson_init(&update);
        BSON_APPEND_DOCUMENT(&update, "$set", &set);

        bson_error_t uerr;
        bool update_ok = false;
        if (!mongoc_collection_update_one(dcoll, &filter, &update,
                                          /*opts=*/NULL, /*reply=*/NULL, &uerr)) {
          WARNING_PRINT("delegate total update failed addr=%.12s… : %s",
                        agg_addr[i], uerr.message);
        } else {
          update_ok = true;
          DEBUG_PRINT("delegate total %s addr=%.12s… total=%lld",
                      have_current ? "updated" : "initialized",
                      agg_addr[i], (long long)new_total);
        }

        bson_destroy(&update);
        bson_destroy(&set);
        bson_destroy(&filter);

        if (update_ok) {
          response_t** responses = NULL;
          char* upd_vote_message = NULL;
          if (build_seed_to_nodes_vote_count_update(agg_addr[i], new_total, &upd_vote_message)) {
            if (xnet_send_data_multi(XNET_DELEGATES_ALL_ONLINE_NOSEEDS, upd_vote_message, &responses)) {
              free(upd_vote_message);
              cleanup_responses(responses);
            } else {
              ERROR_PRINT("Failed to send vote count update message.");
              free(upd_vote_message);
              cleanup_responses(responses);
            }
          } else {
            ERROR_PRINT("Failed to generate vote count update message");
            if (upd_vote_message != NULL) {
              free(upd_vote_message);
            }
          }
        }
      }

      mongoc_collection_destroy(dcoll);
    }
  }

  for (size_t i = 0; i < pay_bucket_count; ++i) {
    const payout_bucket_t* B = &pay_buckets[i];
    const char* delegate_addr = B->delegate;
    const char* ip = NULL;

    // find delegate IP from your online snapshot
    for (size_t k = 0; k < online_count; ++k) {
      if (strcmp(delegates_timer_all[k].public_address, delegate_addr) == 0) {
        ip = delegates_timer_all[k].IP_address;
        break;
      }
    }
    if (!ip) {
      WARNING_PRINT("No online IP for delegate %.12s…; skipping PAYOUT_INSTRUCTION", delegate_addr);
      continue;
    }
    if (B->count == 0) {
      WARNING_PRINT("No outputs for delegate %.12s…; skipping", delegate_addr);
      continue;
    }

    // 1) hash outputs
    uint8_t out_hash[32];
    outputs_digest_sha256(B->outs, B->count, out_hash);
    char out_hash_hex[65];
    bin_to_hex(out_hash, 32, out_hash_hex);  // must NUL-terminate

    // 2) build canonical signable string:
    //    SEED_TO_NODES_PAYOUT|<height>|<blockhash>|<delegate>|<entries>|<outputs_hash>
    char* sign_str = NULL;
    {
      const char* fmt_sign = "SEED_TO_NODES_PAYOUT|%s|%s|%s|%zu|%s";
      int need = snprintf(NULL, 0, fmt_sign,
                          save_block_height,  // e.g. "123456"
                          save_block_hash,    // 64-hex (captured earlier)
                          delegate_addr,
                          B->count,
                          out_hash_hex);
      if (need < 0) {
        ERROR_PRINT("Failed to size signable string");
        continue;
      }
      size_t len = (size_t)need + 1;
      sign_str = (char*)malloc(len);
      if (!sign_str) {
        ERROR_PRINT("malloc(%zu) failed for signable string", len);
        continue;
      }
      int wrote = snprintf(sign_str, len, fmt_sign,
                           save_block_height, save_block_hash, delegate_addr, B->count, out_hash_hex);
      if (wrote < 0 || (size_t)wrote >= len) {
        ERROR_PRINT("snprintf(write) failed or truncated");
        free(sign_str);
        sign_str = NULL;
        continue;
      }
    }

    // 3) sign
    char signature[XCASH_SIGN_DATA_LENGTH + 1] = {0};
    if (!sign_txt_string(sign_str, signature, sizeof signature)) {
      ERROR_PRINT("Failed to sign the payout message for %.12s…", delegate_addr);
      free(sign_str);
      continue;
    }
    free(sign_str);
    sign_str = NULL;

    // 4) build JSON
    sbuf_t sb;
    if (!sbuf_init(&sb, 4096)) {
      ERROR_PRINT("alloc failed building PAYOUT_INSTRUCTION");
      continue;
    }

    if (!sbuf_addf(&sb,
                   "{"
                   "\"type\":\"SEED_TO_NODES_PAYOUT\","
                   "\"block_cutoff\":{\"height\":%s},"
                   "\"delegate_wallet_address\":\"%s\","
                   "\"entries_count\":%zu,"
                   "\"outputs_hash\":\"%s\","
                   "\"XCASH_DPOPS_signature\":\"%s\","
                   "\"outputs\":[",
                   save_block_height, delegate_addr, B->count, out_hash_hex, sig_hex)) {
      free(sb.buf);
      continue;
    }

    for (size_t oi = 0; oi < B->count; ++oi) {
      const payout_output_t* o = &B->outs[oi];
      if (!sbuf_addf(&sb, "%s{\"a\":\"%s\",\"v\":%" PRIu64 "}",
                     (oi ? "," : ""), o->a, (uint64_t)o->v)) {
        free(sb.buf);
        goto next_delegate;
      }
    }

    if (!sbuf_addf(&sb, "]}")) {
      free(sb.buf);
      goto next_delegate;
    }

    // 5) send
    {
      response_t* resp = NULL;
      if (!xnet_send_to_host(ip, sb.buf, &resp)) {
        WARNING_PRINT("send failed to %s for delegate %.12s…", ip, delegate_addr);
      }
      free(sb.buf);
      cleanup_response(resp);
    }

  // fall-through;
  next_delegate:
    ;
  }

  mongoc_client_pool_push(ctx->pool, c);
  free_buckets(pay_buckets, pay_bucket_count);
}

// Just for test
static time_t mk_local_next_every_minutes(int step_min, time_t now) {
  struct tm lt;
  localtime_r(&now, &lt);
  lt.tm_sec = 0;
  int mod = lt.tm_min % step_min;
  int add = (mod == 0) ? step_min : (step_min - mod);
  lt.tm_min += add;
  return mktime(&lt);
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

    WARNING_PRINT("Test mode..................");

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
        if (seed_is_primary()) {
          INFO_PRINT("Scheduler: running PROOF CHECK at %02d:%02d", slot->hour, slot->min);
          run_proof_check(ctx);
        }
      }
    }

#else
    // ---- test dispatch every N minutes ----
    if (is_seed_node) {
      if (seed_is_primary()) {
        WARNING_PRINT("Test scheduler: PROOF CHECK (every %d min)", SCHED_TEST_EVERY_MIN);
        run_proof_check(ctx);
      } else {
        DEBUG_PRINT("Test scheduler: not primary seed — skip proof");
      }
    }
#endif
  }
  return NULL;
}