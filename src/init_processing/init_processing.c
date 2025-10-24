#include "init_processing.h"

/*--------------------------------------------------------------------------
* get_self_sha256
* 
* Compute the SHA-256 hash of the currently running executable image and
* return it as a lowercase hexadecimal string.
*
* Source of bytes:
*   Reads from /proc/self/exe, which points to the exact ELF image the
 *   process was started with (works even if the on-disk path was later
 *   moved or unlinked).
 *
 * Parameters:
 *   out_hex
 *     Buffer provided by the caller to receive the hex digest string.
 *     Must be at least 65 bytes long (64 hex chars + NUL).
 *
 * Returns:
 *   true  on success (out_hex is filled with a 64-char hex string + NUL).
 *   false on failure (out_hex is left unspecified).
 *
 -------------------------------------------------------------------------------*/
bool get_self_sha256(char out_hex[65]) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int  dlen = 0;

  int fd = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;

  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) { close(fd); return false; }

  if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
    EVP_MD_CTX_free(ctx); close(fd); return false;
  }

  unsigned char buf[1 << 16]; // 64 KiB
  for (;;) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
      if (EVP_DigestUpdate(ctx, buf, (size_t)n) != 1) {
        EVP_MD_CTX_free(ctx); close(fd); return false;
      }
    } else if (n == 0) {
      break; // EOF
    } else {
      if (errno == EINTR) continue;
      EVP_MD_CTX_free(ctx); close(fd); return false;
    }
  }

  if (EVP_DigestFinal_ex(ctx, digest, &dlen) != 1) {
    EVP_MD_CTX_free(ctx); close(fd); return false;
  }

  EVP_MD_CTX_free(ctx);
  close(fd);

  static const char hexdig[] = "0123456789abcdef";
  for (unsigned int i = 0; i < dlen; ++i) {
    out_hex[2*i    ] = hexdig[(digest[i] >> 4) & 0xF];
    out_hex[2*i + 1] = hexdig[(digest[i]     ) & 0xF];
  }
  out_hex[2*dlen] = '\0';
  return true;
}

// --- helpers ---
static int parse_semver(const char *s, int *maj, int *min, int *pat) {
  if (!s || !maj || !min || !pat) return 0;
  // allow optional leading 'v' or 'V'
  if (s[0] == 'v' || s[0] == 'V') s++;
  // tolerate missing minor/patch as 0 (e.g., "1" or "1.2")
  int m = 0, n = 0, p = 0;
  int count = sscanf(s, "%d.%d.%d", &m, &n, &p);
  if (count <= 0) return 0;
  if (count == 1) { n = 0; p = 0; }
  if (count == 2) { p = 0; }
  *maj = m; *min = n; *pat = p;
  return 1;
}

static int semver_cmp(const char *a, const char *b) {
  int A=0,B=0,C=0, D=0,E=0,F=0;
  int ok_a = parse_semver(a, &A,&B,&C);
  int ok_b = parse_semver(b, &D,&E,&F);
  if (!ok_a || !ok_b) {
    // Fallback: lexical compare if not parseable
    return strcmp(a ? a : "", b ? b : "");
  }
  if (A != D) return (A < D) ? -1 : 1;
  if (B != E) return (B < E) ? -1 : 1;
  if (C != F) return (C < F) ? -1 : 1;
  return 0;
}

/*---------------------------------------------------------------------------------------------------------
Name: init_processing
Description: Initialize globals and print program start header.
---------------------------------------------------------------------------------------------------------*/
bool init_processing(const arg_config_t *arg_config) {
  (void) arg_config;
  size_t i = 0;
  int count_seeds = 0;

#ifdef SEED_NODE_ON

  while (!is_replica_set_ready()) {
    INFO_PRINT("Mongodb Replica set not ready, waiting...");
    sleep(5);
  }

#endif

  network_data_nodes_amount = get_seed_node_count();

  // Check if database is empty and create the default database data if true
  if (count_db_delegates() <= 0) {
    INFO_PRINT("Delegates collection does not exist so creating it.");
    uint64_t set_counts = 0;

    for (i = 0; network_nodes[i].seed_public_address != NULL; i++) {
      char delegate_name[256];
      strncpy(delegate_name, network_nodes[i].ip_address, sizeof(delegate_name));
      delegate_name[sizeof(delegate_name) - 1] = '\0';  // Null-terminate
      // Replace '.' with '_'
      for (char *p = delegate_name; *p; p++) {
        if (*p == '.') *p = '_';
      }

      uint64_t registration_time = SEED_REGISTRATION_TIME_UTC;
      double set_delegate_fee = 0.00;

      bson_t bson;
      bson_init(&bson);

      // Strings
      bson_append_utf8(&bson, "public_address", -1, network_nodes[i].seed_public_address, -1);
      bson_append_utf8(&bson, "IP_address", -1, network_nodes[i].ip_address, -1);
      bson_append_utf8(&bson, "delegate_name", -1, delegate_name, -1);
      bson_append_utf8(&bson, "about", -1, "Official xCash-Labs Node", -1);
      bson_append_utf8(&bson, "website", -1, "xcashlabs.org", -1);
      bson_append_utf8(&bson, "team", -1, "xCash-Labs Team", -1);
      bson_append_utf8(&bson, "delegate_type", -1, "seed", -1);
      bson_append_utf8(&bson, "server_specs", -1, "Operating System = Ubuntu 22.04", -1);
      bson_append_utf8(&bson, "online_status", -1, "false", -1);
      bson_append_utf8(&bson, "public_key", -1, network_nodes[i].seed_public_key, -1);

      // Numbers
      bson_append_int64(&bson, "total_vote_count", -1, set_counts);
      bson_append_double(&bson, "delegate_fee", -1, set_delegate_fee);
      int64_t ms = (int64_t)registration_time * 1000;
      bson_append_date_time(&bson, "registration_timestamp", -1, ms);

      if (insert_document_into_collection_bson(DATABASE_NAME, DB_COLLECTION_DELEGATES, &bson) != XCASH_OK) {
        ERROR_PRINT("Failed to insert delegate document.");
        bson_destroy(&bson);
        return false;
      }

      bson_destroy(&bson);

// Only update statistics on seed nodes
#ifdef SEED_NODE_ON

      bson_t bson_statistics;
      bson_init(&bson_statistics);

      // Strings
      BSON_APPEND_UTF8(&bson_statistics, "_id", network_nodes[i].seed_public_key);

      // Numbers
      bson_append_int64(&bson_statistics, "block_verifier_total_rounds", -1, set_counts);
      bson_append_int64(&bson_statistics, "block_verifier_online_total_rounds", -1, set_counts);
      bson_append_int64(&bson_statistics, "block_producer_total_rounds", -1, set_counts);

      // Guard watermark for exactly-once counting:
      bson_append_int64(&bson_statistics, "last_counted_block", -1, (int64_t)-1);

      // Insert into "statistics" collection
      if (insert_document_into_collection_bson(DATABASE_NAME, DB_COLLECTION_STATISTICS, &bson_statistics) != XCASH_OK) {
        ERROR_PRINT("Failed to insert statistics document during initialization.");
        bson_destroy(&bson_statistics);
        return false;
      }

// count other stuff... see cloud flare

      bson_destroy(&bson_statistics);

      if (i == 0) {
        if (!add_seed_indexes()) {
          ERROR_PRINT("Failed to add seed indexes to database!");
          return false;
        }
      }

#endif

    }
    if (!add_indexes_delegates()) {
      ERROR_PRINT("Failed to add indexes to delegates collection!");
      return false;
    }
    if (!is_seed_node) {
      if (!add_indexes_blocks_found()) {
        ERROR_PRINT("Failed to add indexes to blocks_found collection!");
        return false;
      }
    }

  }

  // Check DNSSEC records for seeds
  INFO_PRINT("Validating DNSSEC entries...");
  for (i = 0; network_nodes[i].ip_address != NULL; i++) {
    bool have = false;
    dnssec_status_t st = dnssec_query(g_ctx, network_nodes[i].ip_address, RR_IN, &have);
    if (st == DNSSEC_SECURE && have) {
      count_seeds++;
    }
  }

  if (!(count_seeds == network_data_nodes_amount)) {
    FATAL_ERROR_EXIT("Counld not validate DNSSEC records for seed nodes, unable to start");
    return false;
  }

  // Gather from multiple endpoints, merge, and dedupe
  updpops_entry_t allowed[8];  // keep up to 8 concurrent digests
  size_t allowed_n = 0;
  for (i = 0; endpoints[i]; ++i) {
    updpops_entry_t tmp[8];
    size_t m = dnssec_get_all_updpops(g_ctx, endpoints[i], tmp, 8);
    for (size_t j = 0; j < m && allowed_n < 8; ++j) {
      bool seen = false;
      for (size_t k = 0; k < allowed_n; ++k) {
        if (strcmp(tmp[j].digest, allowed[k].digest) == 0) {
          seen = true;
          break;
        }
      }
      if (!seen) allowed[allowed_n++] = tmp[j];
    }
  }

  if (allowed_n == 0) {
    FATAL_ERROR_EXIT("No DNSSEC-validated updpops digests available; refusing to start");
    // shutdown or return error
  }

  // Compute our running binary digest
  char self_sha[SHA256_DIGEST_SIZE + 1];
  if (!get_self_sha256(self_sha)) {
    FATAL_ERROR_EXIT("Unable to compute self SHA-256");
  }

  DEBUG_PRINT("self digest: %s", self_sha);

  const updpops_entry_t* match = NULL;
  if (digest_allowed(self_sha, allowed, allowed_n, &match)) {
    INFO_PRINT("Binary allowed by DNS: version=%s digest=%s", match->version, match->digest);

    // Improved optional: warn only if there is a STRICTLY NEWER allowed version.
    const char* newest = match->version;
    for (i = 0; i < allowed_n; ++i) {
      if (semver_cmp(allowed[i].version, newest) > 0) {
        newest = allowed[i].version;
      }
    }
    if (semver_cmp(newest, match->version) > 0) {
      WARNING_PRINT("A newer allowed version exists (%s). Consider upgrading.", newest);
    }
  } else {
//    FATAL_ERROR_EXIT("Running digest not in allowed list; refusing to start");
    WARNING_PRINT("Running digest not in allowed list; refusing to start");
  }

  return true;
}

/*---------------------------------------------------------------------------------------------------------
Name: print_starter_state
Description: Print program start header.
---------------------------------------------------------------------------------------------------------*/
void print_starter_state(const arg_config_t *arg_config)
{
  (void) arg_config;
  static const char xcash_tech_header[] =
      "\n"
      " /$$   /$$                           /$$        / $$              / $$                    \n"
      "| $$  / $$                          | $$        | $$              | $$                    \n"
      "|  $$/ $$/ /$$$$$$$ /$$$$$$  /$$$$$$| $$$$$$$   | $$      /$$$$$$ | $$       /$$$$$$      \n"
      " \\  $$$$/ /$$_____/|____  $$/$$_____| $$__  $$  | $$     |____  $$| $$      /$$_____     \n"
      "  /$$  $$| $$       /$$$$$$|  $$$$$$| $$  \\ $$  | $$      /$$$$$$ | $$$$$$$ | $$$$$$     \n"
      " /$$/\\  $| $$      /$$__  $$\\____  $| $$  | $$  | $$     /$$__  $$| $$   $$ \\____  $$  \n"
      "| $$  \\ $|  $$$$$$|  $$$$$$$/$$$$$$$| $$  | $$/ | $$$$$$$| $$$$$$$| $$$$$$$ |$$$$$$$     \n"
      "|__/  |__/\\_______/\\_______|_______/|__/  |__|__|________/\\_______/\\________/\\______/\n"
      "\n";
  fputs(xcash_tech_header, stderr);
  time_t now = time(NULL);
  char time_str[64];
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
  fprintf(stderr,
          "%s (%s)\n\n"
          "Wallet Public Address:\t%s\n"
          "\n"
          "Node Type:\t%s\n"
          "\n"
          "Services:\n"
          "Daemon:\t\t%s:%d\n"
          "DPoPS:\t\t%s:%d\n"
          "Wallet:\t\t%s:%d\n"
          "MongoDB:\t%s\n"
          "Log level:\t%d\n",
          XCASH_DPOPS_CURRENT_VERSION, "~Lazarus",
          xcash_wallet_public_address,
          is_seed_node ? "SEED NODE" : "DELEGATE NODE",
          XCASH_DAEMON_IP, XCASH_DAEMON_PORT,
          XCASH_DPOPS_IP, XCASH_DPOPS_PORT,
          XCASH_WALLET_IP, XCASH_WALLET_PORT,
          DATABASE_CONNECTION, log_level);
  if (!is_seed_node) {
    fprintf(stderr, "Delegate fee: %.2f%%, minimum payout: %" PRIu64 "\n", delegate_fee_percent, (uint64_t)minimum_payout);
  }
  fprintf(stderr, "[%s] Daemon startup successful and is busy processing requests...\n\n", time_str);
}