#include "xcash_timer_thread.h"

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

// ---- your actual work (fill these) ----
static void run_proof_check(sched_ctx_t* ctx) {
  mongoc_client_t* c = mongoc_client_pool_pop(ctx->pool);
  if (!c) return;
  // TODO: revalidate proofs / prune invalid
  mongoc_client_pool_push(ctx->pool, c);
}

static void run_payout(sched_ctx_t* ctx) {
  mongoc_client_t* c = mongoc_client_pool_pop(ctx->pool);
  if (!c) return;
  // TODO: build & send payouts (with confirmations, thresholds, idempotency)
  mongoc_client_pool_push(ctx->pool, c);
}

// ---- single scheduler thread ----
void* timer_thread(void* arg) {
  lower_thread_priority_batch();
  sched_ctx_t* ctx = (sched_ctx_t*)arg;
  for (;;) {
    if (atomic_load_explicit(&shutdown_requested, memory_order_relaxed)) break;

    time_t now = time(NULL), run_at;
    int idx = pick_next_slot(now, &run_at);
    if (idx < 0) break;  // shouldn't happen

    time_t wake = run_at - WAKEUP_SKEW_SEC;
    if (wake < now) wake = now;

    // pre-wake, then align to exact minute
    sleep_until(wake);
    if (atomic_load_explicit(&shutdown_requested, memory_order_relaxed)) break;
    sleep_until(run_at);
    if (atomic_load_explicit(&shutdown_requested, memory_order_relaxed)) break;

    // dispatch based on role
    const sched_slot_t* slot = &SLOTS[idx];

    if (slot->kind == JOB_PROOF) {
      if (is_seed_node) {
        INFO_PRINT("Scheduler: running PROOF CHECK at %02d:%02d", slot->hour, slot->min);
        run_proof_check(ctx);
      }
    } else {
      if (!is_seed_node) {
        INFO_PRINT("Scheduler: running PAYOUT at %02d:%02d", slot->hour, slot->min);
        run_payout(ctx);
      }
    }
  }
  return NULL;
}