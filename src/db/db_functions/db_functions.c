#include "db_functions.h"

// Unified resource cleanup function
static inline void free_resources(bson_t* document, bson_t* document2, mongoc_collection_t* collection, mongoc_client_t* database_client_thread) {
  if (document) bson_destroy(document);
  if (document2) bson_destroy(document2);
  if (collection) mongoc_collection_destroy(collection);
  if (database_client_thread) mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
}

// Unified error handling function
static inline int handle_error(const char* message, bson_t* document, bson_t* document2, mongoc_collection_t* collection, mongoc_client_t* database_client_thread) {
  if (message != NULL) {
    ERROR_PRINT("%s", message);
  }
  if (document) bson_destroy(document);
  if (document2) bson_destroy(document2);
  if (collection) mongoc_collection_destroy(collection);
  if (database_client_thread) mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
  return XCASH_ERROR;
}

// Helper function to get a temporary connection
static inline mongoc_client_t* get_temporary_connection(void) {
  if (!database_client_thread_pool) {
    ERROR_PRINT("Database client pool is not initialized!");
    return NULL;
  }
  return mongoc_client_pool_pop(database_client_thread_pool);
}

// Helper function to release a temporary connection
static inline void release_temporary_connection(mongoc_client_t* c) {
  if (c) mongoc_client_pool_push(database_client_thread_pool, c);
}

// Helper function to create a BSON document from JSON
static inline bson_t* create_bson_document(const char* DATA, bson_error_t* error) {
  return bson_new_from_json((const uint8_t*)DATA, -1, error);
}

// Function to count documents in a collection based on a filter
int count_documents_in_collection(const char* DATABASE, const char* COLLECTION, const char* DATA) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return -1;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    ERROR_PRINT("Collection does not exist: %s", COLLECTION);
    free_resources(NULL, NULL, collection, database_client_thread);
    return 0;
  }

  bson_error_t error;
  bson_t* document = create_bson_document(DATA, &error);
  if (!document) return handle_error("Invalid JSON format", NULL, NULL, collection, database_client_thread);

  int count = (int)mongoc_collection_count_documents(collection, document, NULL, NULL, NULL, &error);
  if (count < 0) {
    ERROR_PRINT("Error counting documents in %s: %s", COLLECTION, error.message);
    count = -1;
  }

  free_resources(document, NULL, collection, database_client_thread);
  return count;
}

// Function to count all documents in a collection
int count_all_documents_in_collection(const char* DATABASE, const char* COLLECTION) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return -1;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    ERROR_PRINT("Collection does not exist: %s", COLLECTION);
    free_resources(NULL, NULL, collection, database_client_thread);
    return 0;
  }

  bson_t* filter = bson_new();
  bson_error_t error;
  int count = (int)mongoc_collection_count_documents(collection, filter, NULL, NULL, NULL, &error);
  if (count < 0) ERROR_PRINT("Error counting documents in %s: %s", COLLECTION, error.message);

  bson_destroy(filter);
  free_resources(NULL, NULL, collection, database_client_thread);
  return count;
}

/*-----------------------------------------------------------------------------------------------------------
Name: insert_document_into_collection_bson
Description: inserts a document into an collection
Parameters:
  DATABASE - The database name.
  COLLECTION - The collection name.
  document - document to insert
Return: 0 if an error has occurred or collection does not exist, 1 if successful.
-----------------------------------------------------------------------------------------------------------*/
int insert_document_into_collection_bson(const char* DATABASE, const char* COLLECTION, bson_t* document) {
  if (!DATABASE || !COLLECTION || !document) {
    ERROR_PRINT("insert_document_into_collection_bson: bad params");
    return XCASH_ERROR;
  }

  if (strcmp(COLLECTION, DB_COLLECTION_DELEGATES) == 0) {
    bson_iter_t it;
    const char* id_src = NULL;

    if (bson_iter_init_find(&it, document, "_id")) {
      ERROR_PRINT("Delegates already had _id field, Failed to append BSON document");
      return XCASH_ERROR;
    }

    if (bson_iter_init_find(&it, document, "public_key") && BSON_ITER_HOLDS_UTF8(&it)) {
      id_src = bson_iter_utf8(&it, NULL);
    } else {
      ERROR_PRINT("The public_key field not found, Failed to append BSON document");
      return XCASH_ERROR;
    }

    if (id_src && *id_src) {
      if (!BSON_APPEND_UTF8(document, "_id", id_src)) {
        ERROR_PRINT("Failed to append _id to BSON document.");
        return XCASH_ERROR;
      }
    }
  }

  mongoc_client_t* client = get_temporary_connection();
  if (!client) {
    ERROR_PRINT("Failed to get temporary MongoDB client.");
    return XCASH_ERROR;
  }

  mongoc_collection_t* coll = mongoc_client_get_collection(client, DATABASE, COLLECTION);
  if (!coll) {
    ERROR_PRINT("Failed to get collection '%s.%s'", DATABASE, COLLECTION);
    release_temporary_connection(client);
    return XCASH_ERROR;
  }

  bson_error_t err;
  bool ok = mongoc_collection_insert_one(coll, document, NULL, NULL, &err);

  if (!ok) {
    // Duplicate key? (E11000 / code 11000)
    ERROR_PRINT("Insert failed for %s.%s: domain=%d code=%d msg=%s",
                DATABASE, COLLECTION, err.domain, err.code, err.message);
    mongoc_collection_destroy(coll);
    release_temporary_connection(client);
    return XCASH_ERROR;
  }

  mongoc_collection_destroy(coll);
  release_temporary_connection(client);
  return XCASH_OK;
}

/*-----------------------------------------------------------------------------------------------------------
Name: delegates_apply_vote_total
Description: set the delegate's total_vote_count to the new_total
Parameters:
  DATABASE - The database name.
  COLLECTION - The collection name.
Return: 0 if an error has occurred or collection does not exist, 1 if successful.
-----------------------------------------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------------------------------
Name: delegates_apply_vote_total
Description: Set the delegate's total_vote_count to new_total (no upsert).
Parameters:
  delegate_pubaddr - delegate public address (string key)
  new_total        - absolute total to store (will be clamped to >= 0)
Return:
  true  on success (row existed and was updated or unchanged)
  false on error (DB error or row not found)
-----------------------------------------------------------------------------------------------------------*/
bool delegates_apply_vote_total(const char* delegate_pubaddr, int64_t new_total) {
  if (!delegate_pubaddr || delegate_pubaddr[0] == '\0') {
    ERROR_PRINT("delegates_apply_vote_total: bad params (empty address)");
    return false;
  }

  if (strlen(delegate_pubaddr) != XCASH_WALLET_LENGTH) {
    ERROR_PRINT("delegates_apply_vote_total: invalid address length");
    return false;
  }

  if (new_total < 0) {
    ERROR_PRINT("delegates_apply_vote_total: negative new_total (%lld) clamped to 0",
                (long long)new_total);
    new_total = 0;
  }

  mongoc_client_t* client = get_temporary_connection();  // pool_pop
  if (!client) {
    ERROR_PRINT("Mongo client pool pop failed");
    return false;
  }

  bool ok = false;
  mongoc_collection_t* coll =
      mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_DELEGATES);
  if (!coll) {
    ERROR_PRINT("get_collection failed for %s.%s", DATABASE_NAME, DB_COLLECTION_DELEGATES);
    release_temporary_connection(client);
    return false;
  }

  bson_t filter;
  bson_init(&filter);
  BSON_APPEND_UTF8(&filter, "public_address", delegate_pubaddr);

  bson_t update;
  bson_init(&update);
  bson_t set;
  bson_init(&set);
  BSON_APPEND_INT64(&set, "total_vote_count", new_total);
  BSON_APPEND_DOCUMENT(&update, "$set", &set);

  bson_error_t err;
  bson_t reply;
  bson_init(&reply);

  if (!mongoc_collection_update_one(coll, &filter, &update,
                                    /*opts=*/NULL, &reply, &err)) {
    ERROR_PRINT("update_one $set failed: domain=%d code=%d msg=%s",
                err.domain, err.code, err.message);
    goto cleanup;
  }

  // Ensure a row matched (no upsert)
  int64_t matched = 0;
  bson_iter_t it;
  if (bson_iter_init_find(&it, &reply, "matchedCount")) {
    matched = (BSON_ITER_HOLDS_INT64(&it))   ? bson_iter_int64(&it)
              : (BSON_ITER_HOLDS_INT32(&it)) ? bson_iter_int32(&it)
                                             : 0;
  }
  if (matched == 0) {
    ERROR_PRINT("Delegate not found for absolute vote set: %.12s…", delegate_pubaddr);
    goto cleanup;
  }

  ok = true;

cleanup:
  bson_destroy(&reply);
  bson_destroy(&set);
  bson_destroy(&update);
  bson_destroy(&filter);
  mongoc_collection_destroy(coll);

  release_temporary_connection(client);  // pool_push
  return ok;
}

/*-----------------------------------------------------------------------------------------------------------
Name: check_if_database_collection_exist
Description: Checks if a database collection exists.
Parameters:
  DATABASE - The database name.
  COLLECTION - The collection name.
Return: 0 if an error has occurred or collection does not exist, 1 if successful.
-----------------------------------------------------------------------------------------------------------*/
int check_if_database_collection_exist(const char* DATABASE, const char* COLLECTION) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_database_t* database = mongoc_client_get_database(database_client_thread, DATABASE);
  if (!database) return handle_error("Failed to get database", NULL, NULL, NULL, database_client_thread);

  bson_error_t error;
  bool collection_exists = mongoc_database_has_collection(database, COLLECTION, &error);

  mongoc_database_destroy(database);
  mongoc_client_pool_push(database_client_thread_pool, database_client_thread);

  if (!collection_exists) {
    if (error.message[0] != '\0') {
      ERROR_PRINT("MongoDB error: %s", error.message);
    } else {
      ERROR_PRINT("Collection does not exist: %s", COLLECTION);
    }
    return XCASH_ERROR;
  }
  return XCASH_OK;
}

// Function to read a specific field from a document
int read_document_field_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, const char* FIELD_NAME, char* result, size_t result_size) {
  if (!DATABASE || !COLLECTION || !DATA || !FIELD_NAME || !result || result_size == 0) {
    fprintf(stderr, "Invalid input parameters.\n");
    return XCASH_ERROR;
  }

  const bson_t* current_document;
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    free_resources(NULL, NULL, collection, database_client_thread);
    return XCASH_ERROR;
  }

  bson_error_t error;
  bson_t* document = create_bson_document(DATA, &error);
  if (!document) {
    handle_error("Invalid JSON format", NULL, NULL, collection, database_client_thread);
    return XCASH_ERROR;
  }

  mongoc_cursor_t* document_settings = mongoc_collection_find_with_opts(collection, document, NULL, NULL);
  int found = 0;

  while (mongoc_cursor_next(document_settings, &current_document)) {
    bson_iter_t iter;
    if (bson_iter_init_find(&iter, current_document, FIELD_NAME)) {
      const char* value = bson_iter_utf8(&iter, NULL);
      if (value) {
        strncpy(result, value, result_size - 1);
        result[result_size - 1] = '\0';  // Ensure null-termination
        found = 1;
        break;
      }
    }
  }

  mongoc_cursor_destroy(document_settings);
  bson_destroy(document);
  free_resources(NULL, NULL, collection, database_client_thread);

  if (!found) {
    ERROR_PRINT("Field '%s' not found in document.", FIELD_NAME);
    return XCASH_ERROR;
  }

  return XCASH_OK;
}

// Function to update a single document in a collection
int update_document_from_collection_bson(const char* DATABASE, const char* COLLECTION, const bson_t* filter, const bson_t* update_fields) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    return handle_error("Collection does not exist", NULL, NULL, collection, database_client_thread);
  }

  bson_error_t error;
  bson_t update_doc;
  bson_init(&update_doc);
  BSON_APPEND_DOCUMENT(&update_doc, "$set", update_fields);

  if (!mongoc_collection_update_one(collection, filter, &update_doc, NULL, NULL, &error)) {
    bson_destroy(&update_doc);
    return handle_error("Failed to update document", NULL, NULL, collection, database_client_thread);
  }

  bson_destroy(&update_doc);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
  return XCASH_OK;
}

// Function to delete a document from a collection
int delete_document_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    return handle_error("Collection does not exist", NULL, NULL, collection, database_client_thread);
  }

  bson_error_t error;
  bson_t* document = create_bson_document(DATA, &error);
  if (!document) return handle_error("Invalid JSON format", NULL, NULL, collection, database_client_thread);

  if (!mongoc_collection_delete_one(collection, document, NULL, NULL, &error)) {
    return handle_error("Failed to delete document", document, NULL, collection, database_client_thread);
  }

  free_resources(document, NULL, collection, database_client_thread);
  return XCASH_OK;
}

bool is_replica_set_ready(void) {
  bson_t reply;
  bson_error_t error;
  bool is_ready = false;

  mongoc_client_t* client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) return false;

  bson_t* cmd = BCON_NEW("replSetGetStatus", BCON_INT32(1));
  if (mongoc_client_command_simple(client, "admin", cmd, NULL, &reply, &error)) {
    bson_iter_t iter;
    if (bson_iter_init_find(&iter, &reply, "myState")) {
      int32_t state = bson_iter_int32(&iter);
      // MongoDB states: 1 = PRIMARY, 2 = SECONDARY
      if (state == 1 || state == 2) {
        is_ready = true;
      }
    }
  } else {
    WARNING_PRINT("Could not run replSetGetStatus: %s", error.message);
  }

  bson_destroy(&reply);
  bson_destroy(cmd);
  mongoc_client_pool_push(database_client_thread_pool, client);
  return is_ready;
}

// Function to determin if this seed delegate is the primary mongodb node
bool seed_is_primary(void) {
  char ip_address[IP_LENGTH + 1] = {0};
  int i = 0;
  while (i < network_data_nodes_amount) {
    if (strcmp(network_nodes[i].seed_public_address, xcash_wallet_public_address) == 0) {
      if (!hostname_to_ip(network_nodes[i].ip_address, ip_address, sizeof(ip_address))) {
        ERROR_PRINT("Could not resolve %s", network_nodes[i].ip_address);
        return false;
      }
      break;
    }
    i++;
  }

  mongoc_client_t* client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) return false;

  bool ok = false;
  bson_error_t err;
  bson_t reply;
  bson_t* cmd = BCON_NEW("hello", BCON_INT32(1));  // ask the node who it is

  if (mongoc_client_command_simple(client, "admin", cmd, NULL, &reply, &err)) {
    bson_iter_t iter;
    if (bson_iter_init(&iter, &reply) &&
        bson_iter_find_case(&iter, "me") && BSON_ITER_HOLDS_UTF8(&iter)) {
      const char* me = bson_iter_utf8(&iter, NULL);  // e.g. "10.0.0.5:27017" or "[::1]:27017"
      char ip[256];
      size_t n = 0;

      if (me[0] == '[') {
        const char* rb = strchr(me, ']');
        n = rb ? (size_t)(rb - me - 1) : 0;
        memcpy(ip, me + 1, n);
      } else {
        const char* c = strrchr(me, ':');
        n = c ? (size_t)(c - me) : strlen(me);
        memcpy(ip, me, n);
      }

      ip[n] = '\0';
      if (strcmp(ip, ip_address) == 0) {
        ok = true;
      }
    }
    bson_destroy(&reply);
  } else {
    ERROR_PRINT("hello failed: %s", err.message);
  }

  bson_destroy(cmd);
  mongoc_client_pool_push(database_client_thread_pool, client);
  return ok;
}

bool add_seed_indexes(void) {
  bson_error_t err;
  bool ok = true;

  mongoc_client_t* client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) return false;

  /* =========================
     STATISTICS COLLECTION
     ========================= */
  {
    mongoc_collection_t* coll =
        mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_STATISTICS);

    // compound index: {_id:1, last_counted_block:1}
    bson_t keys, opts;
    bson_init(&keys);
    bson_init(&opts);
    BSON_APPEND_INT32(&keys, "_id", 1);
    BSON_APPEND_INT32(&keys, "last_counted_block", 1);
    BSON_APPEND_UTF8(&opts, "name", "idx_id_last_counted_block");

    mongoc_index_model_t* m = mongoc_index_model_new(&keys, &opts);

    bson_t create_opts;
    bson_init(&create_opts);
    // keep these if you're on a replica set; omit commitQuorum on standalone
    BSON_APPEND_UTF8(&create_opts, "commitQuorum", "majority");
    BSON_APPEND_INT32(&create_opts, "maxTimeMS", 15000);

    bson_t reply;
    bson_init(&reply);
    if (!mongoc_collection_create_indexes_with_opts(coll, &m, 1, &create_opts, &reply, &err)) {
      char* json = bson_as_canonical_extended_json(&reply, NULL);
      fprintf(stderr, "[indexes] statistics failed: %s\nDetails: %s\n",
              err.message, json ? json : "(no reply)");
      if (json) bson_free(json);
    }

    // cleanup
    bson_destroy(&reply);
    bson_destroy(&create_opts);
    mongoc_index_model_destroy(m);
    bson_destroy(&opts);
    bson_destroy(&keys);
    mongoc_collection_destroy(coll);
  }

  /* =========================
     CONSENSUS_ROUNDS COLLECTION
     ========================= */
  {
    const int32_t TTL_SEC = 60 * 60 * 24 * 182;  // 182 days (6 months)

    mongoc_database_t* db = mongoc_client_get_database(client, DATABASE_NAME);

    // Ensure collection exists (ignore NamespaceExists = 48)
    bson_t c_opts;
    bson_init(&c_opts);
    if (!mongoc_database_create_collection(db, DB_COLLECTION_ROUNDS, &c_opts, &err)) {
      if (err.code != 48) {
        ok = false;
        WARNING_PRINT("[indexes] create_collection %s failed (domain=%d code=%d): %s",
                      DB_COLLECTION_ROUNDS, err.domain, err.code, err.message);
      }
    }
    bson_destroy(&c_opts);

    mongoc_collection_t* coll =
        mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_ROUNDS);
    if (!coll) {
      ok = false;
      WARNING_PRINT("[indexes] get_collection %s failed", DB_COLLECTION_ROUNDS);
      mongoc_database_destroy(db);
      // return or goto as fits your flow
    } else {
      // 1) unique block_height
      bson_t rk1, ro1;
      bson_init(&rk1);
      bson_init(&ro1);
      BSON_APPEND_INT32(&rk1, "block_height", 1);
      BSON_APPEND_UTF8(&ro1, "name", "ux_block_height");
      BSON_APPEND_BOOL(&ro1, "unique", true);
      mongoc_index_model_t* rm1 = mongoc_index_model_new(&rk1, &ro1);

      // 2) TTL on ts_decided
      bson_t rk2, ro2;
      bson_init(&rk2);
      bson_init(&ro2);
      BSON_APPEND_INT32(&rk2, "ts_decided", 1);  // single-field TTL index
      BSON_APPEND_UTF8(&ro2, "name", "ttl_ts_decided_365d");
      BSON_APPEND_INT32(&ro2, "expireAfterSeconds", TTL_SEC);
      mongoc_index_model_t* rm2 = mongoc_index_model_new(&rk2, &ro2);

      mongoc_index_model_t* rmodels[] = {rm1, rm2};

      bson_t create_opts;
      bson_init(&create_opts);
      BSON_APPEND_UTF8(&create_opts, "commitQuorum", "majority");
      BSON_APPEND_INT32(&create_opts, "maxTimeMS", 15000);

      bson_t reply;
      bson_init(&reply);
      if (!mongoc_collection_create_indexes_with_opts(
              coll, rmodels, 2, &create_opts, &reply, &err)) {
        ok = false;
        char* json = bson_as_canonical_extended_json(&reply, NULL);
        fprintf(stderr, "[indexes] consensus_rounds create_indexes failed: %s\nDetails: %s\n",
                err.message, json ? json : "(no reply)");
        if (json) bson_free(json);
      }

      // cleanup
      bson_destroy(&reply);
      bson_destroy(&create_opts);
      mongoc_index_model_destroy(rm2);
      mongoc_index_model_destroy(rm1);
      bson_destroy(&ro2);
      bson_destroy(&rk2);
      bson_destroy(&ro1);
      bson_destroy(&rk1);
      mongoc_collection_destroy(coll);
      mongoc_database_destroy(db);
    }
  }

  /* =========================
   RESERVE_PROOFS COLLECTION
   ========================= */
  {
    mongoc_collection_t* coll =
        mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_RESERVE_PROOFS);

    // --- index on public_address_voted_for ---
    bson_t k1, o1;
    bson_init(&k1);
    bson_init(&o1);
    BSON_APPEND_INT32(&k1, "public_address_voted_for", 1);
    BSON_APPEND_UTF8(&o1, "name", "idx_voted_for");
    mongoc_index_model_t* m1 = mongoc_index_model_new(&k1, &o1);

    mongoc_index_model_t* models[] = {m1};

    bson_t create_opts;
    bson_init(&create_opts);
    BSON_APPEND_UTF8(&create_opts, "commitQuorum", "majority");
    BSON_APPEND_INT32(&create_opts, "maxTimeMS", 15000);

    // writeConcern: majority
    bson_t wc;
    bson_init(&wc);
    BSON_APPEND_UTF8(&wc, "w", "majority");
    BSON_APPEND_DOCUMENT(&create_opts, "writeConcern", &wc);

    bson_t reply;
    bson_error_t ierr;
    bson_init(&reply);

    if (!mongoc_collection_create_indexes_with_opts(
            coll, models, (int)(sizeof(models) / sizeof(models[0])),
            &create_opts, &reply, &ierr)) {
      char* json = bson_as_canonical_extended_json(&reply, NULL);
      if (!(strstr(ierr.message, "already exists") ||
            (json && strstr(json, "already exists")))) {
        ok = false;
        fprintf(stderr, "[indexes] %s failed: %s\nDetails: %s\n",
                DB_COLLECTION_RESERVE_PROOFS, ierr.message, json ? json : "(no reply)");
      }
      if (json) bson_free(json);
    }

    // cleanup
    bson_destroy(&reply);
    bson_destroy(&wc);
    bson_destroy(&create_opts);

    mongoc_index_model_destroy(m1);
    bson_destroy(&o1);
    bson_destroy(&k1);

    mongoc_collection_destroy(coll);
  }

  mongoc_client_pool_push(database_client_thread_pool, client);
  return ok;
}

bool add_indexes(void) {
  bson_error_t err;
  bool ok = true;

  mongoc_client_t* client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) return false;

  /* =========================
     DELEGATES COLLECTION
     ========================= */
  {
    mongoc_collection_t* coll =
        mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_DELEGATES);

    // 1) unique public_address
    bson_t k1, o1;
    bson_init(&k1);
    bson_init(&o1);
    BSON_APPEND_INT32(&k1, "public_address", 1);
    BSON_APPEND_UTF8(&o1, "name", "uniq_public_address");
    BSON_APPEND_BOOL(&o1, "unique", true);
    mongoc_index_model_t* m1 = mongoc_index_model_new(&k1, &o1);

    // 2) unique public_key
    bson_t k2, o2;
    bson_init(&k2);
    bson_init(&o2);
    BSON_APPEND_INT32(&k2, "public_key", 1);
    BSON_APPEND_UTF8(&o2, "name", "uniq_public_key");
    BSON_APPEND_BOOL(&o2, "unique", true);
    mongoc_index_model_t* m2 = mongoc_index_model_new(&k2, &o2);

    // 3) unique delegate_name (case-insensitive via collation)
    bson_t k3, o3, coll3;
    bson_init(&k3);
    bson_init(&o3);
    bson_init(&coll3);
    BSON_APPEND_INT32(&k3, "delegate_name", 1);
    BSON_APPEND_UTF8(&o3, "name", "uniq_delegate_name_ci");
    BSON_APPEND_BOOL(&o3, "unique", true);
    BSON_APPEND_UTF8(&coll3, "locale", "en");
    BSON_APPEND_INT32(&coll3, "strength", 2);  // case-insensitive, diacritics-insensitive
    BSON_APPEND_DOCUMENT(&o3, "collation", &coll3);
    mongoc_index_model_t* m3 = mongoc_index_model_new(&k3, &o3);

    // 4) unique IP_address (only if you truly want one delegate per IP/host)
    bson_t k4, o4;
    bson_init(&k4);
    bson_init(&o4);
    BSON_APPEND_INT32(&k4, "IP_address", 1);
    BSON_APPEND_UTF8(&o4, "name", "uniq_IP_address");
    BSON_APPEND_BOOL(&o4, "unique", true);
    mongoc_index_model_t* m4 = mongoc_index_model_new(&k4, &o4);

    mongoc_index_model_t* models[] = {m1, m2, m3, m4};

    bson_t create_opts;
    bson_init(&create_opts);
    if (is_seed_node) {
      BSON_APPEND_UTF8(&create_opts, "commitQuorum", "majority");
    }
    BSON_APPEND_INT32(&create_opts, "maxTimeMS", 15000);

    bson_t reply;
    bson_init(&reply);
    if (!mongoc_collection_create_indexes_with_opts(coll, models, 4, &create_opts, &reply, &err)) {
      ok = false;
      char* json = bson_as_canonical_extended_json(&reply, NULL);
      fprintf(stderr, "[indexes] delegates failed: %s\nDetails: %s\n",
              err.message, json ? json : "(no reply)");
      if (json) bson_free(json);
    }

    // cleanup
    bson_destroy(&reply);
    bson_destroy(&create_opts);
    mongoc_index_model_destroy(m4);
    mongoc_index_model_destroy(m3);
    mongoc_index_model_destroy(m2);
    mongoc_index_model_destroy(m1);
    bson_destroy(&o4);
    bson_destroy(&k4);
    bson_destroy(&coll3);
    bson_destroy(&o3);
    bson_destroy(&k3);
    bson_destroy(&o2);
    bson_destroy(&k2);
    bson_destroy(&o1);
    bson_destroy(&k1);
    mongoc_collection_destroy(coll);
  }

  /* =========================
     BLOCKS_FOUND COLLECTION
     ========================= */
  // don't add to seed nodes
#ifndef SEED_NODE_ON

  {
    mongoc_collection_t* coll =
        mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_BLOCKS_FOUND);

    // --- unique index on block_height ---
    bson_t k1, o1;
    bson_init(&k1);
    bson_init(&o1);
    BSON_APPEND_INT32(&k1, "block_height", 1);
    BSON_APPEND_UTF8(&o1, "name", "u_block_height");
    BSON_APPEND_BOOL(&o1, "unique", true);
    mongoc_index_model_t* m1 = mongoc_index_model_new(&k1, &o1);

    // --- queue index: (processed, block_height) ---
    bson_t k2, o2;
    bson_init(&k2);
    bson_init(&o2);
    BSON_APPEND_INT32(&k2, "processed", 1);
    BSON_APPEND_INT32(&k2, "block_height", 1);
    BSON_APPEND_UTF8(&o2, "name", "processed_block_height_idx");
    mongoc_index_model_t* m2 = mongoc_index_model_new(&k2, &o2);

    // NOTE: non-const array of pointers
    mongoc_index_model_t* models[] = {m1, m2};

    bson_t create_opts;
    bson_init(&create_opts);
    BSON_APPEND_INT32(&create_opts, "maxTimeMS", 15000);

    bson_t reply;
    bson_init(&reply);
    if (!mongoc_collection_create_indexes_with_opts(coll, models, 2, &create_opts, &reply, &err)) {
      ok = false;
      char* json = bson_as_canonical_extended_json(&reply, NULL);
      fprintf(stderr, "[indexes] delegates failed: %s\nDetails: %s\n",
              err.message, json ? json : "(no reply)");
      if (json) bson_free(json);
    }

    // cleanup
    bson_destroy(&reply);
    bson_destroy(&create_opts);
    mongoc_index_model_destroy(m2);
    bson_destroy(&o2);
    bson_destroy(&k2);
    mongoc_index_model_destroy(m1);
    bson_destroy(&o1);
    bson_destroy(&k1);
    mongoc_collection_destroy(coll);
  }

#endif

  mongoc_client_pool_push(database_client_thread_pool, client);
  return ok;
}

int count_db_delegates(void) {
  bson_error_t error;
  bool result = false;
  int64_t count;

  result = db_count_doc(DATABASE_NAME, collection_names[XCASH_DB_DELEGATES], &count, &error);
  if (!result) {
    count = -1;
  }
  return count;
}

int count_recs(const bson_t* recs) {
  bson_iter_t iter;
  int count = 0;

  if (bson_iter_init(&iter, recs)) {
    while (bson_iter_next(&iter)) {
      count++;
    }
  }
  return count;
}

// from db_operations

bool db_export_collection_to_bson(const char* db_name, const char* collection_name, bson_t* out, bson_error_t* error) {
  if (!out) {
    ERROR_PRINT("Output BSON pointer is NULL");
    return false;
  }

  bson_init(out);

  bson_t filter = BSON_INITIALIZER;
  bool success = db_find_doc(db_name, collection_name, &filter, out, error, false);
  bson_destroy(&filter);
  return success;
}

bool db_find_all_doc(const char* db_name, const char* collection_name, bson_t* reply, bson_error_t* error) {
  bson_t filter = BSON_INITIALIZER;
  bool result = db_find_doc(db_name, collection_name, &filter, reply, error, true);
  bson_destroy(&filter);
  return result;
}

bool db_find_doc(const char* db_name, const char* collection_name, const bson_t* query, bson_t* reply,
                 bson_error_t* error, bool exclude_id) {
  if (!reply) {
    ERROR_PRINT("db_find_doc: 'reply' is NULL");
    return false;
  }

  mongoc_client_t* client;
  mongoc_collection_t* collection;
  mongoc_cursor_t* cursor;
  const bson_t* doc = NULL;
  bson_t* opts = NULL;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    ERROR_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    ERROR_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  if (exclude_id) {
    opts = BCON_NEW("projection", "{", "_id", BCON_BOOL(false), "}");
  }

  // Find documents
  cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
  if (opts) bson_destroy(opts);

  if (!cursor) {
    ERROR_PRINT("Failed to initiate find operation");
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  int index = 0;
  char str_index[16];  // for converting integer to string
  while (mongoc_cursor_next(cursor, &doc)) {
    snprintf(str_index, sizeof(str_index), "%d", index);
    bson_append_document(reply, str_index, -1, doc);
    index++;
  }

  if (index == 0) {
    ERROR_PRINT("Query returned no documents");
  }

  if (mongoc_cursor_error(cursor, error)) {
    ERROR_PRINT("Cursor error: %s", error->message);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);

    return false;
  }

  // Cleanup
  mongoc_cursor_destroy(cursor);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return true;
}

bool db_upsert_doc(const char* db_name, const char* collection_name, const bson_t* doc, bson_error_t* error) {
  mongoc_client_t* client;
  mongoc_collection_t* collection;
  bson_iter_t iter;
  bool result = true;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    ERROR_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    ERROR_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  bson_t* opts = BCON_NEW("upsert", BCON_BOOL(true));
  bson_t query = BSON_INITIALIZER;

  // Check if the document is single record
  if (bson_iter_init_find(&iter, doc, "_id")) {
    bson_append_value(&query, "_id", -1, bson_iter_value(&iter));

    if (!mongoc_collection_replace_one(collection, &query, doc, opts, NULL, error)) {
      ERROR_PRINT("Failed to upsert document: %s", error->message);
      result = false;
    }
  } else {
    char* str = bson_as_legacy_extended_json(doc, NULL);
    ERROR_PRINT("Failed to find '_id' in upsert document: %s", str);
    free(str);

    result = false;
  }

  // Cleanup
  bson_destroy(&query);
  bson_destroy(opts);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return result;
}

bool db_upsert_multi_docs(const char* db_name, const char* collection_name, const bson_t* docs, bson_error_t* error) {
  mongoc_client_t* client;
  mongoc_collection_t* collection;
  bson_iter_t iter;
  bool result = true;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    ERROR_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    ERROR_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  bson_t* opts = BCON_NEW("upsert", BCON_BOOL(true));

  if (bson_iter_init(&iter, docs)) {
    bson_iter_t child;
    while (bson_iter_next(&iter)) {
      bson_t query = BSON_INITIALIZER;
      const uint8_t* data;
      uint32_t len;
      bson_t sub_doc;

      bson_iter_document(&iter, &len, &data);
      bson_init_static(&sub_doc, data, len);

      if (bson_iter_init_find(&child, &sub_doc, "_id")) {
        bson_append_value(&query, "_id", -1, bson_iter_value(&child));
      } else {
        char* str = bson_as_legacy_extended_json(&sub_doc, NULL);
        ERROR_PRINT("Failed to find '_id' in upsert document: %s", str);
        free(str);

        result = false;
        bson_destroy(&query);
        break;
      }

      if (!mongoc_collection_replace_one(collection, &query, &sub_doc, opts, NULL, error)) {
        ERROR_PRINT("Failed to upsert document: %s", error->message);
        result = false;
        bson_destroy(&query);
        break;
      }
      bson_destroy(&query);
    }
  }

  // Cleanup
  bson_destroy(opts);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return result;
}

bool db_delete_doc(const char* db_name, const char* collection_name, const bson_t* query, bson_error_t* error) {
  mongoc_client_t* client;
  mongoc_collection_t* collection;
  bson_t opts = BSON_INITIALIZER;
  bool result;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    ERROR_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    ERROR_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  // Delete documents
  result = mongoc_collection_delete_many(collection, query, &opts, NULL, error);
  if (!result) {
    ERROR_PRINT("Failed to delete documents: %s", error->message);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  // Cleanup
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return true;
}

bool db_drop(const char* db_name, const char* collection_name, bson_error_t* error) {
  mongoc_client_t* client;
  mongoc_collection_t* collection;
  bool result;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    ERROR_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    ERROR_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  result = mongoc_collection_drop(collection, error);
  if (!result) {
    ERROR_PRINT("Can't drop %s, error: %s", collection_name, error->message);
  }

  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return result;
}

bool db_count_doc(const char* db_name, const char* collection_name, int64_t* result_count, bson_error_t* error) {
  mongoc_client_t* client;
  mongoc_collection_t* collection;
  int64_t count;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    ERROR_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    ERROR_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  bson_t* filter = bson_new();  // empty filter

  count = mongoc_collection_count_documents(collection, filter, NULL, NULL, NULL, error);
  if (count < 0) {
    ERROR_PRINT("Failed to count documents: %s", error->message);
    bson_destroy(filter);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  // Cleanup
  bson_destroy(filter);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  *result_count = count;
  return true;
}

// Returns true if a reserve_proofs doc exists for voter_id.
// On success: *total_out set, delegate_name_out filled ("" if unknown).
bool get_vote_total_and_delegate_name(
    const char* voter_id,
    int64_t* total_out,
    char delegate_name_out[MAXIMUM_BUFFER_SIZE_DELEGATES_NAME + 1]) {
  if (!voter_id || !*voter_id || !total_out || !delegate_name_out) {
    ERROR_PRINT("get_vote_total_and_delegate_name: bad params");
    return false;
  }

  *total_out = 0;
  delegate_name_out[0] = '\0';

  mongoc_client_t* c = mongoc_client_pool_pop(database_client_thread_pool);
  if (!c) {
    ERROR_PRINT("Mongo client pool pop failed");
    return false;
  }

  bool ok = false;

  // ---- Step 1: reserve_proofs by _id (voter) ----
  mongoc_collection_t* rp =
      mongoc_client_get_collection(c, DATABASE_NAME, DB_COLLECTION_RESERVE_PROOFS);
  if (!rp) {
    ERROR_PRINT("get_collection failed for %s.%s", DATABASE_NAME, DB_COLLECTION_RESERVE_PROOFS);
    goto CLEANUP;
  }

  bson_t f = BSON_INITIALIZER;
  BSON_APPEND_UTF8(&f, "_id", voter_id);

  bson_t* opts = BCON_NEW(
      "projection", "{",
      "total_vote", BCON_BOOL(true),
      "public_address_voted_for", BCON_BOOL(true),
      "_id", BCON_BOOL(false),
      "}");

  mongoc_cursor_t* cur = mongoc_collection_find_with_opts(rp, &f, opts, NULL);
  if (opts) bson_destroy(opts);

  const bson_t* doc = NULL;
  char delegate_addr[XCASH_WALLET_LENGTH + 1];
  delegate_addr[0] = '\0';

  if (!cur || !mongoc_cursor_next(cur, &doc)) {
    goto CLEANUP_F_AND_CUR;
  }

  // extract total + delegate address
  {
    bson_iter_t it;
    if (bson_iter_init_find(&it, doc, "total_vote") && BSON_ITER_HOLDS_INT64(&it))
      *total_out = bson_iter_int64(&it);

    if (bson_iter_init_find(&it, doc, "public_address_voted_for") && BSON_ITER_HOLDS_UTF8(&it)) {
      const char* s = bson_iter_utf8(&it, NULL);
      if (s) snprintf(delegate_addr, sizeof(delegate_addr), "%s", s);
    }
  }

CLEANUP_F_AND_CUR:
  if (cur) {
    mongoc_cursor_destroy(cur);
    cur = NULL;
  }
  bson_destroy(&f);

  // ---- Step 2: delegates by public_address -> delegate_name ----
  if (delegate_addr[0]) {
    mongoc_collection_t* del =
        mongoc_client_get_collection(c, DATABASE_NAME, DB_COLLECTION_DELEGATES);
    if (!del) {
      ERROR_PRINT("get_collection failed for %s.%s", DATABASE_NAME, DB_COLLECTION_DELEGATES);
      goto CLEANUP;
    }

    bson_t df = BSON_INITIALIZER;
    BSON_APPEND_UTF8(&df, "public_address", delegate_addr);

    bson_t* dopts = BCON_NEW(
        "projection", "{",
        "delegate_name", BCON_BOOL(true),
        "_id", BCON_BOOL(false),
        "}");

    cur = mongoc_collection_find_with_opts(del, &df, dopts, NULL);
    if (dopts) bson_destroy(dopts);

    const bson_t* ddoc = NULL;
    if (cur && mongoc_cursor_next(cur, &ddoc)) {
      bson_iter_t it;
      if (bson_iter_init_find(&it, ddoc, "delegate_name") && BSON_ITER_HOLDS_UTF8(&it)) {
        const char* nm = bson_iter_utf8(&it, NULL);
        if (nm) snprintf(delegate_name_out, MAXIMUM_BUFFER_SIZE_DELEGATES_NAME, "%s", nm);
      }
    }

    if (cur) {
      mongoc_cursor_destroy(cur);
      cur = NULL;
    }
    bson_destroy(&df);
    mongoc_collection_destroy(del);
  }

  ok = true;

CLEANUP:
  mongoc_client_pool_push(database_client_thread_pool, c);
  return ok;
}

// Gets public_address_voted_for, total_vote, and reserve_proof for a voter _id.
// Returns true on success (doc found), false otherwise.
bool fetch_reserve_proof_fields_by_id(
    const char* voter_public_address,
    char* voted_for_out, size_t voted_for_sz,
    int64_t* total_out,
    char* reserve_proof_out, size_t rp_sz,
    bson_error_t* err) {
  if (!voter_public_address || !voted_for_out || !total_out || !reserve_proof_out) return false;

  mongoc_client_t* c = mongoc_client_pool_pop(database_client_thread_pool);
  if (!c) return false;

  bool ok = false;
  mongoc_collection_t* coll =
      mongoc_client_get_collection(c, DATABASE_NAME, DB_COLLECTION_RESERVE_PROOFS);

  bson_t filter = BSON_INITIALIZER;
  BSON_APPEND_UTF8(&filter, "_id", voter_public_address);

  // Projection: only what you need
  bson_t proj = BSON_INITIALIZER;
  BSON_APPEND_BOOL(&proj, "public_address_voted_for", true);
  BSON_APPEND_BOOL(&proj, "total_vote", true);
  BSON_APPEND_BOOL(&proj, "reserve_proof", true);
  BSON_APPEND_BOOL(&proj, "_id", false);

  bson_t opts = BSON_INITIALIZER;
  BSON_APPEND_DOCUMENT(&opts, "projection", &proj);
  BSON_APPEND_INT64(&opts, "limit", 1);  // we only need one

  mongoc_cursor_t* cur = mongoc_collection_find_with_opts(coll, &filter, &opts, NULL);

  const bson_t* doc = NULL;
  if (cur && mongoc_cursor_next(cur, &doc)) {
    bson_iter_t it;

    // public_address_voted_for
    if (bson_iter_init_find(&it, doc, "public_address_voted_for") && BSON_ITER_HOLDS_UTF8(&it)) {
      const char* s = bson_iter_utf8(&it, NULL);
      if (s) snprintf(voted_for_out, voted_for_sz, "%s", s);
    } else {
      voted_for_out[0] = '\0';
    }

    // total (or total_vote)
    if (bson_iter_init_find(&it, doc, "total_vote") &&
        (BSON_ITER_HOLDS_INT64(&it) || BSON_ITER_HOLDS_INT32(&it))) {
      *total_out = BSON_ITER_HOLDS_INT64(&it) ? bson_iter_int64(&it) : bson_iter_int32(&it);
    } else {
      *total_out = 0;
    }

    // reserve_proof
    if (bson_iter_init_find(&it, doc, "reserve_proof") && BSON_ITER_HOLDS_UTF8(&it)) {
      const char* rp = bson_iter_utf8(&it, NULL);
      if (rp) {
        strncpy(reserve_proof_out, rp, rp_sz - 1);
        reserve_proof_out[rp_sz - 1] = '\0';
      } else {
        reserve_proof_out[0] = '\0';
      }
    } else {
      reserve_proof_out[0] = '\0';
    }

    ok = true;
  }

  if (cur && !ok && err) mongoc_cursor_error(cur, err);

  if (cur) mongoc_cursor_destroy(cur);
  bson_destroy(&opts);
  bson_destroy(&proj);
  bson_destroy(&filter);
  mongoc_collection_destroy(coll);
  mongoc_client_pool_push(database_client_thread_pool, c);
  return ok;
}

/*---------------------------------------------------------------------------------------------------------
Name: get_delegate_fee
Description: Retrieves `delegate_fee` (double) from the collections table for the current wallet.
Parameters:
  out_fee - [out] Receives the delegate fee as a double (e.g., 5.0 for 5%)
Return:  XCASH_OK (1) on success, XCASH_ERROR (0) if missing / not a double / error
---------------------------------------------------------------------------------------------------------*/
int get_delegate_fee(double* out_fee)
{
  if (!out_fee) {
    ERROR_PRINT("get_delegate_fee: out_fee is NULL");
    return XCASH_ERROR;
  }

  mongoc_client_t* client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    ERROR_PRINT("get_delegate_fee: client pool pop failed");
    return XCASH_ERROR;
  }

  mongoc_collection_t* coll =
      mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_DELEGATES);
  if (!coll) {
    ERROR_PRINT("get_delegate_fee: get_collection failed");
    mongoc_client_pool_push(database_client_thread_pool, client);
    return XCASH_ERROR;
  }

  // Filter: { public_address: "<xcash_wallet_public_address>" }
  bson_t filter; bson_init(&filter);
  BSON_APPEND_UTF8(&filter, "public_address", xcash_wallet_public_address);

  // Projection: { delegate_fee: 1 }
  bson_t proj; bson_init(&proj);
  BSON_APPEND_INT32(&proj, "delegate_fee", 1);

  bson_t opts; bson_init(&opts);
  BSON_APPEND_DOCUMENT(&opts, "projection", &proj);
  BSON_APPEND_INT32(&opts, "limit", 1);

  mongoc_cursor_t* cur = mongoc_collection_find_with_opts(coll, &filter, &opts, NULL);
  const bson_t* doc = NULL;
  double fee = 0.0;
  bool ok = false;

  if (cur && mongoc_cursor_next(cur, &doc)) {
    bson_iter_t it;
    if (bson_iter_init_find(&it, doc, "delegate_fee") &&
        bson_iter_type(&it) == BSON_TYPE_DOUBLE) {
      fee = bson_iter_double(&it);
      ok = true;
    } else {
      WARNING_PRINT("Delegate_fee is not stored in the correct format");
    }
  }

  if (cur) mongoc_cursor_destroy(cur);
  bson_destroy(&opts);
  bson_destroy(&proj);
  bson_destroy(&filter);
  mongoc_collection_destroy(coll);
  mongoc_client_pool_push(database_client_thread_pool, client);

  if (!ok) {
    ERROR_PRINT("get_delegate_fee: delegate_fee not found as double for %s",
                xcash_wallet_public_address);
    return XCASH_ERROR;
  }

  *out_fee = fee;
  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name:        compute_payouts_due
Description: 
  Aggregates unprocessed block rewards below `in_block_height` from the `found_blocks`
  collection, caps the payable total by the wallet's `in_unlocked_balance`, and
  distributes the capped sum proportionally across `parsed[0..entries_count-1]`
  using floor math (no overpay, no fractions). For each address, it upserts into
  `DB_COLLECTION_PAYOUT_BALANCES` (e.g., "payout_balances") by incrementing
  `pending_atomic` and setting `updated_at`. After accrual succeeds, all eligible
  `found_blocks` below `in_block_height` are marked `processed:true`.

  Notes:
    - If no unprocessed rewards or sum is zero after capping, the function exits early.
    - Any remainder from floor division is left unpaid (by design).
    - Uses the global `database_client_thread_pool` (libmongoc).
    - All Mongo resources are cleaned on every path.

Parameters:
  parsed            Pointer to an array of payout_output_t (must have fields:
                    `a` = address, `v` = vote weight).
  in_block_height   Only `found_blocks` with `block_height < in_block_height` and
                    `processed:false` are considered. (Pass your "current − N confirmations"
                    height here.)
  in_unlocked_balance
                    Unlocked wallet balance (atomic units) that caps total payouts.
  entries_count     Number of elements in `parsed`.

Return:
  XCASH_OK (1)   on success (including the case where nothing is payable).
  XCASH_ERROR (0) on any database error or arithmetic overflow/invalid input
                  (e.g., NULL `parsed` with non-zero `entries_count`, vote-sum overflow,
                  Mongo failures).
---------------------------------------------------------------------------------------------------------*/
int compute_payouts_due(payout_output_t *parsed, uint64_t in_block_height, int64_t in_unlocked_balance, size_t entries_count)                        
{
  int rc = XCASH_OK;

  if (!parsed || entries_count == 0) {
    ERROR_PRINT("No parsed entries");
    return XCASH_ERROR;
  }

  mongoc_client_t* client = NULL;
  mongoc_collection_t* coll_blocks = NULL;     // found_blocks
  mongoc_collection_t* coll_pub    = NULL;     // public_addresses
  mongoc_cursor_t* cur = NULL;
  bson_t* pipeline = NULL;
  const bson_t* doc = NULL;

  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    ERROR_PRINT("compute_payouts_due: mongoc_client_pool_pop failed");
    return XCASH_ERROR;
  }

  /* ---- SUM unprocessed rewards ---- */
  coll_blocks = mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_BLOCKS_FOUND);
  if (!coll_blocks) {
    ERROR_PRINT("compute_payouts_due: get_collection '%s.%s' failed", DATABASE_NAME, DB_COLLECTION_BLOCKS_FOUND);
    rc = XCASH_ERROR;
    goto done;
  }

  /* Build pipeline via JSON to avoid BCON varargs pitfalls */
  {
    char jbuf[VVSMALL_BUFFER_SIZE];
    int n = snprintf(
        jbuf, sizeof jbuf,
        "[{\"$match\":{\"block_height\":{\"$lt\":{\"$numberLong\":\"%" PRIu64 "\"}},"
        "\"processed\":false}},"
        "{\"$group\":{\"_id\":null,\"total\":{\"$sum\":\"$block_reward\"}}}]",
        (uint64_t)in_block_height);
    if (n < 0 || (size_t)n >= sizeof jbuf) {
      ERROR_PRINT("compute_payouts_due: pipeline JSON snprintf failed/overflow");
      rc = XCASH_ERROR;
      goto done;
    }

    bson_error_t jerr;
    pipeline = bson_new_from_json((const uint8_t*)jbuf, -1, &jerr);
    if (!pipeline) {
      ERROR_PRINT("compute_payouts_due: bson_new_from_json failed: %s", jerr.message);
      rc = XCASH_ERROR;
      goto done;
    }
  }

  cur = mongoc_collection_aggregate(coll_blocks, MONGOC_QUERY_NONE, pipeline, NULL, NULL);
  if (!cur) {
    ERROR_PRINT("compute_payouts_due: aggregate cursor creation failed");
    rc = XCASH_ERROR;
    goto done;
  }

  int64_t sum = 0;
  if (mongoc_cursor_next(cur, &doc)) {
    bson_iter_t it;
    if (!bson_iter_init_find(&it, doc, "total")) {
      ERROR_PRINT("compute_payouts_due: aggregation missing 'total'");
      rc = XCASH_ERROR;
      goto done;
    }
    sum = bson_iter_as_int64(&it);
    if (sum < 0) sum = 0;
  }

  {
    bson_error_t err;
    if (mongoc_cursor_error(cur, &err)) {
      ERROR_PRINT("compute_payouts_due: MongoDB error: %s", err.message);
      rc = XCASH_ERROR;
      goto done;
    }
  }

  // release aggregation resources early
  if (cur) { mongoc_cursor_destroy(cur); cur = NULL; }
  if (pipeline) { bson_destroy(pipeline); pipeline = NULL; }
  if (coll_blocks) { mongoc_collection_destroy(coll_blocks); coll_blocks = NULL; }

  // ---- Convert percent -> bps (clamped), then compute fee + send_total (all integer math)
  long long bps_ll;
  if (delegate_fee_percent >= 0.0)
    bps_ll = (long long)(delegate_fee_percent * 100.0 + 0.5);
  else
    bps_ll = (long long)(delegate_fee_percent * 100.0 - 0.5);

  if (bps_ll < 0) bps_ll = 0;
  if (bps_ll > 10000) bps_ll = 10000;
  uint32_t bps = (uint32_t)bps_ll;

  // fee_atomic = round_half_up(sum * bps / 10000)
  uint64_t fee_atomic;
  {
    // Use 128-bit for the product to avoid overflow: sum can be large.
    unsigned __int128 prod = (unsigned __int128)sum * (uint64_t)bps;
    fee_atomic = (uint64_t)((prod + (BPS_SCALE / 2)) / BPS_SCALE);
  }

  uint64_t send_total = sum - fee_atomic;
  sum = send_total;

  // ---- Funds cap check (post-fee)
  if (sum > in_unlocked_balance) {
    ERROR_PRINT("compute_payouts_due: Not enough funds (need %" PRIu64 ", have %" PRIu64 ")",
                sum, in_unlocked_balance);
    rc = XCASH_ERROR;
    goto done;
  }

  /* ---- Vote totals ---- */
  uint64_t total_delegate_votes = 0;
  for (size_t k = 0; k < entries_count; ++k) {
    if (UINT64_MAX - total_delegate_votes < parsed[k].v) {
      ERROR_PRINT("vote sum overflow at index %zu", k);
      rc = XCASH_ERROR;
      goto done;
    }
    total_delegate_votes += parsed[k].v;
  }
  if (entries_count == 0 || total_delegate_votes == 0) {
    ERROR_PRINT("No entries or total_delegate_votes == 0");
    rc = XCASH_ERROR;
    goto done;
  }

  uint64_t safe_unlocked = (in_unlocked_balance > 0) ? (uint64_t)in_unlocked_balance : 0;
  uint64_t pending_sum   = (sum > 0) ? (uint64_t)sum : 0;
  uint64_t sum_atomic    = (pending_sum < safe_unlocked) ? pending_sum : safe_unlocked;
  if (sum_atomic == 0) {
    DEBUG_PRINT("Nothing to pay (sum_atomic == 0)");
    rc = XCASH_OK;
    goto done;
  }

  //jed
  WARNING_PRINT("sum_atomic=%" PRIu64, sum_atomic);

  /* ---- Accrue per-address pending in public_addresses ---- */
  coll_pub = mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_PAYOUT_BALANCES);
  if (!coll_pub) {
    ERROR_PRINT("compute_payouts_due: get_collection '%s.%s' failed", DATABASE_NAME, DB_COLLECTION_PAYOUT_BALANCES);
    rc = XCASH_ERROR;
    goto done;
  }

  uint64_t base_sum = 0;
  for (size_t k = 0; k < entries_count; ++k) {
    const char* addr = parsed[k].a;
    uint64_t pay = (uint64_t)(((__uint128_t)sum_atomic * parsed[k].v) / total_delegate_votes);

    if (UINT64_MAX - base_sum < pay) {
      ERROR_PRINT("payout accumulation overflow");
      rc = XCASH_ERROR;
      goto done;
    }
    base_sum += pay;

    if (pay == 0) continue;

    bson_t q, u, opts, inc, set;
    bson_error_t err;

    bson_init(&q);
    BSON_APPEND_UTF8(&q, "_id", addr);

    bson_init(&u);
    BSON_APPEND_DOCUMENT_BEGIN(&u, "$inc", &inc);
    BSON_APPEND_INT64(&inc, "pending_atomic", (int64_t)pay);
    bson_append_document_end(&u, &inc);

    BSON_APPEND_DOCUMENT_BEGIN(&u, "$set", &set);
    BSON_APPEND_DATE_TIME(&set, "updated_at", (int64_t)time(NULL) * 1000);
    bson_append_document_end(&u, &set);

    bson_init(&opts);
    BSON_APPEND_BOOL(&opts, "upsert", true);

    bool ok = mongoc_collection_update_one(coll_pub, &q, &u, &opts, NULL, &err);

    bson_destroy(&opts);
    bson_destroy(&u);
    bson_destroy(&q);

    if (!ok) {
      ERROR_PRINT("public_addresses upsert failed for %s: %s", addr, err.message);
      rc = XCASH_ERROR;
      goto done;
    }
  }

  /* ---- Mark eligible found_blocks as processed ---- */
  {
    coll_blocks = mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_BLOCKS_FOUND);
    if (!coll_blocks) {
      ERROR_PRINT("finalize: get_collection '%s.%s' failed", DATABASE_NAME, DB_COLLECTION_BLOCKS_FOUND);
      rc = XCASH_ERROR;
      goto done;
    }

    bson_t filter;
    bson_init(&filter);
    bson_t lt;
    BSON_APPEND_DOCUMENT_BEGIN(&filter, "block_height", &lt);
    BSON_APPEND_INT64(&lt, "$lt", (int64_t)in_block_height);
    bson_append_document_end(&filter, &lt);
    BSON_APPEND_BOOL(&filter, "processed", false);

    bson_t update;
    bson_init(&update);
    bson_t set;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$set", &set);
    BSON_APPEND_BOOL(&set, "processed", true);
    bson_append_document_end(&update, &set);

    bson_t reply;
    bson_init(&reply);
    bson_error_t err;
    bool ok = mongoc_collection_update_many(coll_blocks, &filter, &update, NULL, &reply, &err);
    if (!ok) {
      ERROR_PRINT("finalize: update_many failed: %s", err.message);
      rc = XCASH_ERROR;
    } else {
      bson_iter_t it;
      int64_t n = 0;
      if (bson_iter_init_find(&it, &reply, "modifiedCount") && BSON_ITER_HOLDS_INT32(&it))
        n = bson_iter_int32(&it);
      else if (bson_iter_init_find(&it, &reply, "nModified") && BSON_ITER_HOLDS_INT32(&it))
        n = bson_iter_int32(&it);
      // jed
      WARNING_PRINT("finalize: marked %" PRIi64 " found_blocks as processed", n);
    }

    bson_destroy(&reply);
    bson_destroy(&update);
    bson_destroy(&filter);
    mongoc_collection_destroy(coll_blocks);
    coll_blocks = NULL;
  }

done:
  if (coll_pub) mongoc_collection_destroy(coll_pub);
  if (cur) mongoc_cursor_destroy(cur);
  if (pipeline) bson_destroy(pipeline);
  if (coll_blocks) mongoc_collection_destroy(coll_blocks);
  if (client) mongoc_client_pool_push(database_client_thread_pool, client);
  return rc;
}

/*---------------------------------------------------------------------------------------------------------
Name: run_payout_sweep_simple
Purpose:
  Streams over payout_balances and pays any address that either:
    (1) has pending_atomic >= minimum_payout_atomic, OR
    (2) last activity (updated_at) is older than NO_ACTIVITY_DELETE and pending_atomic > 0.
  For each paid address:
    - Sends a single-recipient wallet-rpc `transfer` with subtract_fee_from_outputs:[0]
    - Inserts an immutable receipt into DB_COLLECTION_PAYOUT_RECEIPTS (lean schema)
    - Zeroes pending_atomic (or deletes the doc if "stale_7d" path)
  On the first failure (wallet send, receipt insert, or DB update), returns XCASH_ERROR.

Inputs (globals expected):
  DATABASE_NAME
  DB_COLLECTION_PAYOUT_BALANCES      // "payout_balances"
  DB_COLLECTION_PAYOUT_RECEIPTS      // "payout_receipts"
  minimum_payout                     // XCASH (double or int)
  XCASH_ATOMIC_UNITS                 // 1 XCASH = e.g., 1_000_000
  NO_ACTIVITY_DELETE                 // ms (e.g., 7 days)
  database_client_thread_pool        // mongoc pool
  TRANSACTION_HASH_LENGTH            // must be 64

Writes:
  Collection: DB_COLLECTION_PAYOUT_RECEIPTS (per payout)
    _id:                tx_hash (string, 64 hex)
    payment_address:    recipient address (string)
    amount_atomic_sent: wallet-reported amount (if available) or requested 'pend' (int64)
    tx_fee_atomic:      wallet-reported tx fee (int64)
    created_at:         server time (BSON date ms)

Cursor & safety:
  - Streams using find_with_opts (projection + batchSize + noCursorTimeout)
  - No in-memory arrays; one doc at a time
  - Early return on first error to avoid partial ambiguous state

Idempotency:
  - If a payout was already sent (same tx_hash) and receipt exists, insert may return DUPKEY.
    You may treat duplicate-key as success if desired.

Return:
  XCASH_OK on full success; XCASH_ERROR on first failure.
---------------------------------------------------------------------------------------------------------*/
int run_payout_sweep_simple(void)
{
  int rc = XCASH_OK;

  const int64_t minimum_payout_atomic = (int64_t)(minimum_payout * XCASH_ATOMIC_UNITS);

  mongoc_client_t* client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    ERROR_PRINT("run_payout_sweep_simple: mongoc_client_pool_pop failed");
    return XCASH_ERROR;
  }

  const int64_t now_ms = (int64_t)time(NULL) * 1000;
  const int64_t cutoff = now_ms - NO_ACTIVITY_DELETE;

  // Build query: (pending >= min) OR (stale AND pending > 0)
  bson_t query; bson_init(&query);
  bson_t or_arr; bson_append_array_begin(&query, "$or", -1, &or_arr);
  { // or[0]: pending_atomic >= min
    bson_t d, c; bson_append_document_begin(&or_arr, "0", -1, &d);
      bson_append_document_begin(&d, "pending_atomic", -1, &c);
        BSON_APPEND_INT64(&c, "$gte", minimum_payout_atomic);
      bson_append_document_end(&d, &c);
    bson_append_document_end(&or_arr, &d);
  }
  { // or[1]: updated_at < cutoff AND pending_atomic > 0
    bson_t d, and_arr, a0, a1, c0, c1; bson_append_document_begin(&or_arr, "1", -1, &d);
      bson_append_array_begin(&d, "$and", -1, &and_arr);
        bson_append_document_begin(&and_arr, "0", -1, &a0);
          bson_append_document_begin(&a0, "updated_at", -1, &c0);
            BSON_APPEND_DATE_TIME(&c0, "$lt", cutoff);
          bson_append_document_end(&a0, &c0);
        bson_append_document_end(&and_arr, &a0);

        bson_append_document_begin(&and_arr, "1", -1, &a1);
          bson_append_document_begin(&a1, "pending_atomic", -1, &c1);
            BSON_APPEND_INT64(&c1, "$gt", 0);
          bson_append_document_end(&a1, &c1);
        bson_append_document_end(&and_arr, &a1);
      bson_append_array_end(&d, &and_arr);
    bson_append_document_end(&or_arr, &d);
  }
  bson_append_array_end(&query, &or_arr);

  // Projection + cursor opts
  bson_t opts; bson_init(&opts);
  bson_t proj; bson_init(&proj);
  BSON_APPEND_INT32(&proj, "_id", 1);
  BSON_APPEND_INT32(&proj, "pending_atomic", 1);
  BSON_APPEND_INT32(&proj, "updated_at", 1);
  BSON_APPEND_DOCUMENT(&opts, "projection", &proj);
  BSON_APPEND_BOOL(&opts, "noCursorTimeout", true);
  BSON_APPEND_INT32(&opts, "batchSize", 500);

  mongoc_collection_t* coll_bal =
      mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_PAYOUT_BALANCES);
  if (!coll_bal) {
    ERROR_PRINT("run_payout_sweep_simple: get_collection %s failed", DB_COLLECTION_PAYOUT_BALANCES);
    bson_destroy(&query); bson_destroy(&opts); bson_destroy(&proj);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return XCASH_ERROR;
  }

  mongoc_collection_t* coll_pay =
      mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_PAYOUT_RECEIPTS);
  if (!coll_pay) {
    ERROR_PRINT("run_payout_sweep_simple: get_collection %s failed", DB_COLLECTION_PAYOUT_RECEIPTS);
    bson_destroy(&query); bson_destroy(&opts); bson_destroy(&proj);
    mongoc_collection_destroy(coll_bal);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return XCASH_ERROR;
  }

  mongoc_cursor_t* cur = mongoc_collection_find_with_opts(coll_bal, &query, &opts, NULL);
  bson_destroy(&query); bson_destroy(&opts); bson_destroy(&proj);

  const bson_t* doc;
  size_t processed = 0;

  while (mongoc_cursor_next(cur, &doc)) {
    processed++;

    // Parse fields
    bson_iter_t it;
    const char* addr = NULL;
    int64_t pend = 0, updated = 0;

    if (bson_iter_init_find(&it, doc, "_id") && BSON_ITER_HOLDS_UTF8(&it))
      addr = bson_iter_utf8(&it, NULL);
    if (bson_iter_init_find(&it, doc, "pending_atomic") && BSON_ITER_HOLDS_INT64(&it))
      pend = bson_iter_int64(&it);
    if (bson_iter_init_find(&it, doc, "updated_at") && BSON_ITER_HOLDS_DATE_TIME(&it))
      updated = bson_iter_date_time(&it);

    if (!addr) { ERROR_PRINT("run_payout_sweep_simple: skipping doc missing _id"); continue; }

    const bool meets_min   = (pend >= minimum_payout_atomic);
    const bool is_stale_7d = (updated > 0 && (now_ms - updated) >= NO_ACTIVITY_DELETE);
    if (!meets_min && !(is_stale_7d && pend > 0)) continue;

    const char* reason = meets_min ? "stake reward min_threshold" : "stake reward stale_7d";
    const bool delete_after = (!meets_min && is_stale_7d);

    // --- Wallet send  ---
    char first_hash[TRANSACTION_HASH_LENGTH + 1] = {0};
    char split_siblings[MAX_SIBLINGS][TRANSACTION_HASH_LENGTH + 1] = {{0}};
    uint64_t fee = 0, amt_sent = 0;
    int64_t ts = now_ms;
    size_t split_siblings_count = 0;

    sleep(1);
    int send_ok = wallet_payout_send(addr, pend, reason, first_hash, sizeof(first_hash), &fee, &ts, &amt_sent,
      split_siblings, MAX_SIBLINGS, &split_siblings_count);
    if (send_ok == XCASH_ERROR) {
        ERROR_PRINT("run_payout_sweep_simple: payout failed for %s amount=%" PRId64, addr, pend);
        rc = XCASH_ERROR;
        goto done;
      }

    // --- Insert payment record ---
    {
      bson_error_t err;
      bson_t pay_doc;
      bson_init(&pay_doc);
      BSON_APPEND_UTF8(&pay_doc, "_id", first_hash);
      BSON_APPEND_UTF8(&pay_doc, "payment_address", addr);
      BSON_APPEND_INT64(&pay_doc, "amount_atomic_requested", (int64_t)pend);
      BSON_APPEND_INT64(&pay_doc, "amount_atomic_sent", (int64_t)amt_sent);
      BSON_APPEND_INT64(&pay_doc, "tx_fee_atomic", (int64_t)fee);
      BSON_APPEND_DATE_TIME(&pay_doc, "created_at", ts /* or ts_ms if that's your timestamp var */);
      BSON_APPEND_INT32(&pay_doc, "split_count", (int32_t)split_siblings_count);  // counts for quick queries
      // siblings array: split_tx_ids: [ "<txid2>", "<txid3>", ... ]
      if (split_siblings_count > 0) {
        bson_t arr;
        bson_append_array_begin(&pay_doc, "split_tx_ids", -1, &arr);
        for (uint32_t i = 0; i < split_siblings_count; ++i) {
          const char* key;
          char keybuf[16];
          bson_uint32_to_string(i, &key, keybuf, sizeof(keybuf));  // keys: "0","1",...
          BSON_APPEND_UTF8(&arr, key, split_siblings[i]);
        }
        bson_append_array_end(&pay_doc, &arr);
      }

      if (!mongoc_collection_insert_one(coll_pay, &pay_doc, NULL, NULL, &err)) {
        ERROR_PRINT("run_payout_sweep_simple: payment insert failed for %s (tx=%s): %s",
                    addr, first_hash, err.message);
        bson_destroy(&pay_doc);
        rc = XCASH_ERROR;
        goto done;
      }

      // jed
      WARNING_PRINT("payout recorded: _id=%s split=%zu fee=%" PRIu64 " sent=%" PRIu64,
                 first_hash, split_siblings_count, fee, (uint64_t)amt_sent);

      bson_destroy(&pay_doc);
    }

    // --- Zero or delete source record ---
    {
      bson_error_t err;
      bool ok = false;

      if (delete_after) {
        bson_t q; bson_init(&q);
        BSON_APPEND_UTF8(&q, "_id", addr);
        ok = mongoc_collection_delete_one(coll_bal, &q, NULL, NULL, &err);
        bson_destroy(&q);
      } else {
        bson_t q, u, set;
        bson_init(&q); BSON_APPEND_UTF8(&q, "_id", addr);
        bson_init(&u);
        BSON_APPEND_DOCUMENT_BEGIN(&u, "$set", &set);
          BSON_APPEND_INT64(&set, "pending_atomic", 0);
          BSON_APPEND_DATE_TIME(&set, "updated_at", now_ms);
        bson_append_document_end(&u, &set);
        ok = mongoc_collection_update_one(coll_bal, &q, &u, NULL, NULL, &err);
        bson_destroy(&q); bson_destroy(&u);
      }

      if (!ok) {
        ERROR_PRINT("run_payout_sweep_simple: DB modify failed for %s after payout; code=%d msg=%s",
                    addr, err.code, err.message);
        rc = XCASH_ERROR;
        goto done;
      }
    }

    // jed
    WARNING_PRINT("run_payout_sweep_simple: paid %" PRId64 " to %s (%s); %s [tx=%s fee=%" PRIu64 "]",
               pend, addr, reason, delete_after ? "deleted" : "zeroed", first_hash, fee);
  }

  if (mongoc_cursor_error(cur, NULL)) {
    ERROR_PRINT("run_payout_sweep_simple: cursor error scanning payout_balances");
    rc = XCASH_ERROR;
    goto done;
  }

done:
  if (cur) mongoc_cursor_destroy(cur);
  if (coll_pay) mongoc_collection_destroy(coll_pay);
  if (coll_bal) mongoc_collection_destroy(coll_bal);
  if (client) mongoc_client_pool_push(database_client_thread_pool, client);

  if (rc == XCASH_OK) {
    DEBUG_PRINT("run_payout_sweep_simple: completed, processed=%zu", processed);
  }
  return rc;
}