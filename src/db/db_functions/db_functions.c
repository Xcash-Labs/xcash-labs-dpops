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
    DEBUG_PRINT("Collection does not exist: %s", COLLECTION);
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
    DEBUG_PRINT("Collection does not exist: %s", COLLECTION);
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
Name: delegates_apply_vote_delta
Description: increment a delegate's total_vote_count by delta (can be negative)
Parameters:
  DATABASE - The database name.
  COLLECTION - The collection name.
Return: 0 if an error has occurred or collection does not exist, 1 if successful.
-----------------------------------------------------------------------------------------------------------*/
bool delegates_apply_vote_delta(const char* delegate_pubaddr, int64_t delta) {
  if (!delegate_pubaddr || !*delegate_pubaddr || delta == 0) {
    ERROR_PRINT("delegates_apply_vote_delta: bad params");
    return false;
  }

  mongoc_client_t* client = get_temporary_connection(); // pool_pop
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

  // filter: { public_address: <delegate_pubaddr> }
  bson_t filter; bson_init(&filter);
  BSON_APPEND_UTF8(&filter, "public_address", delegate_pubaddr);

  bson_t update; bson_init(&update);
  bson_t inc; bson_init(&inc);
  BSON_APPEND_INT64(&inc, "total_vote_count", delta);
  BSON_APPEND_DOCUMENT(&update, "$inc", &inc);

  // No upsert → we want to fail if the doc doesn't exist
  bson_t* opts = NULL; // (optionally add writeConcern below)

  bson_error_t err;
  bson_t reply; bson_init(&reply);
  if (!mongoc_collection_update_one(coll, &filter, &update, opts, &reply, &err)) {
    ERROR_PRINT("update_one failed: domain=%d code=%d msg=%s", err.domain, err.code, err.message);
    goto done;
  }

  // Check matchedCount to ensure the row existed
  int64_t matched = 0;
  {
    bson_iter_t it;
    if (bson_iter_init_find(&it, &reply, "matchedCount")) {
      if (BSON_ITER_HOLDS_INT32(&it)) matched = bson_iter_int32(&it);
      else if (BSON_ITER_HOLDS_INT64(&it)) matched = bson_iter_int64(&it);
    }
  }
  if (matched == 0) {
    ERROR_PRINT("Delegate not found for vote delta: %s", delegate_pubaddr);
    goto done;
  }

  ok = true;

done:
  bson_destroy(&reply);
  bson_destroy(&inc);
  bson_destroy(&update);
  bson_destroy(&filter);
  mongoc_collection_destroy(coll);
  release_temporary_connection(client); // pool_push
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
      DEBUG_PRINT("Collection does not exist: %s", COLLECTION);
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

  mongoc_client_t *client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) return false;

  bson_t *cmd = BCON_NEW("replSetGetStatus", BCON_INT32(1));
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


bool seed_is_primary(void) {
  bson_t reply;
  bson_error_t error;
  bool is_primary = false;

  mongoc_client_t *client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) return false;

  bson_error_t err;
  bson_t reply;
  bool ok = false;
  bson_t* cmd = BCON_NEW("hello", BCON_INT32(1));
  if (mongoc_client_command_simple(c, "admin", cmd, NULL, &reply, &err)) {
    bson_iter_t it;
    if (bson_iter_init(&it, &reply) &&
        bson_iter_find_case(&it, "me") && BSON_ITER_HOLDS_UTF8(&it)) {
      const char* me = bson_iter_utf8(&it, NULL);
      INFO_PRINT("Current connected ip: %s", me);
    }
  }

  bson_destroy(&reply);
  bson_destroy(cmd);
  mongoc_client_pool_push(database_client_thread_pool, client);
  return is_primary;

}

bool add_seed_indexes(void) {
  bson_error_t err;
  bool ok = true;

  mongoc_client_t *client = mongoc_client_pool_pop(database_client_thread_pool);
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
    const int32_t TTL_SEC = 60 * 60 * 24 * 365;  // 365 days

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

  mongoc_client_t *client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) return false;

  /* =========================
     DELEGATES COLLECTION
     ========================= */
  {
    mongoc_collection_t *coll =
        mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_DELEGATES);

    // 1) unique public_address
    bson_t k1, o1; bson_init(&k1); bson_init(&o1);
    BSON_APPEND_INT32(&k1, "public_address", 1);
    BSON_APPEND_UTF8(&o1, "name", "uniq_public_address");
    BSON_APPEND_BOOL(&o1, "unique", true);
    mongoc_index_model_t *m1 = mongoc_index_model_new(&k1, &o1);

    // 2) unique public_key
    bson_t k2, o2; bson_init(&k2); bson_init(&o2);
    BSON_APPEND_INT32(&k2, "public_key", 1);
    BSON_APPEND_UTF8(&o2, "name", "uniq_public_key");
    BSON_APPEND_BOOL(&o2, "unique", true);
    mongoc_index_model_t *m2 = mongoc_index_model_new(&k2, &o2);

    // 3) unique delegate_name (case-insensitive via collation)
    bson_t k3, o3, coll3; bson_init(&k3); bson_init(&o3); bson_init(&coll3);
    BSON_APPEND_INT32(&k3, "delegate_name", 1);
    BSON_APPEND_UTF8(&o3, "name", "uniq_delegate_name_ci");
    BSON_APPEND_BOOL(&o3, "unique", true);
    BSON_APPEND_UTF8(&coll3, "locale", "en");
    BSON_APPEND_INT32(&coll3, "strength", 2); // case-insensitive, diacritics-insensitive
    BSON_APPEND_DOCUMENT(&o3, "collation", &coll3);
    mongoc_index_model_t *m3 = mongoc_index_model_new(&k3, &o3);

    // 4) unique IP_address (only if you truly want one delegate per IP/host)
    bson_t k4, o4; bson_init(&k4); bson_init(&o4);
    BSON_APPEND_INT32(&k4, "IP_address", 1);
    BSON_APPEND_UTF8(&o4, "name", "uniq_IP_address");
    BSON_APPEND_BOOL(&o4, "unique", true);
    mongoc_index_model_t *m4 = mongoc_index_model_new(&k4, &o4);

    mongoc_index_model_t *models[] = { m1, m2, m3, m4 };

    bson_t create_opts; bson_init(&create_opts);
    if (is_seed_node) {
      BSON_APPEND_UTF8(&create_opts, "commitQuorum", "majority");
    }
    BSON_APPEND_INT32(&create_opts, "maxTimeMS", 15000);

    bson_t reply; bson_init(&reply);
    if (!mongoc_collection_create_indexes_with_opts(coll, models, 4, &create_opts, &reply, &err)) {
      ok = false;
      char *json = bson_as_canonical_extended_json(&reply, NULL);
      fprintf(stderr, "[indexes] delegates failed: %s\nDetails: %s\n",
              err.message, json ? json : "(no reply)");
      if (json) bson_free(json);
    }

    // cleanup
    bson_destroy(&reply);
    bson_destroy(&create_opts);
    mongoc_index_model_destroy(m4); mongoc_index_model_destroy(m3);
    mongoc_index_model_destroy(m2); mongoc_index_model_destroy(m1);
    bson_destroy(&o4); bson_destroy(&k4);
    bson_destroy(&coll3); bson_destroy(&o3); bson_destroy(&k3);
    bson_destroy(&o2); bson_destroy(&k2);
    bson_destroy(&o1); bson_destroy(&k1);
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
    BSON_APPEND_INT32(&k1, "block_height", 1);        // key: block_height ascending
    BSON_APPEND_UTF8(&o1, "name", "u_block_height");  // index name
    BSON_APPEND_BOOL(&o1, "unique", true);            // unique constraint
    mongoc_index_model_t* m1 = mongoc_index_model_new(&k1, &o1);

    mongoc_index_model_t* models[] = {m1};

    // createIndexes options (standalone: no commitQuorum / writeConcern)
    bson_t create_opts;
    bson_init(&create_opts);
    BSON_APPEND_INT32(&create_opts, "maxTimeMS", 15000);

    // run createIndexes
    bson_t reply;
    bson_error_t ierr;
    bson_init(&reply);

    if (!mongoc_collection_create_indexes_with_opts(
            coll, models, (int)(sizeof(models) / sizeof(models[0])),
            &create_opts, &reply, &ierr)) {
      char* json = bson_as_canonical_extended_json(&reply, NULL);
      if (!(strstr(ierr.message, "already exists") ||
            (json && strstr(json, "already exists")))) {
        ok = false;  // assumes 'ok' exists in your surrounding scope
        fprintf(stderr, "[indexes] %s failed: %s\nDetails: %s\n",
                DB_COLLECTION_BLOCKS_FOUND, ierr.message, json ? json : "(no reply)");
      }
      if (json) bson_free(json);
    }

    // cleanup
    bson_destroy(&reply);
    bson_destroy(&create_opts);

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

int count_recs(const bson_t *recs) {
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

bool db_find_all_doc(const char *db_name, const char *collection_name, bson_t *reply, bson_error_t *error) {
  bson_t filter = BSON_INITIALIZER;
  bool result = db_find_doc(db_name, collection_name, &filter, reply, error, true);
  bson_destroy(&filter);
  return result;
}

bool db_find_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_t *reply,
                 bson_error_t *error, bool exclude_id) {

  if (!reply) {
    ERROR_PRINT("db_find_doc: 'reply' is NULL");
    return false;
  }

  mongoc_client_t *client;
  mongoc_collection_t *collection;
  mongoc_cursor_t *cursor;
  const bson_t *doc = NULL;
  bson_t *opts = NULL;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
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
    DEBUG_PRINT("Failed to initiate find operation");
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
    DEBUG_PRINT("Query returned no documents");
  }

  if (mongoc_cursor_error(cursor, error)) {
    DEBUG_PRINT("Cursor error: %s", error->message);
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

bool db_upsert_doc(const char *db_name, const char *collection_name, const bson_t *doc, bson_error_t *error) {
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  bson_iter_t iter;
  bool result = true;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  bson_t *opts = BCON_NEW("upsert", BCON_BOOL(true));
  bson_t query = BSON_INITIALIZER;

  // Check if the document is single record
  if (bson_iter_init_find(&iter, doc, "_id")) {
    bson_append_value(&query, "_id", -1, bson_iter_value(&iter));

    if (!mongoc_collection_replace_one(collection, &query, doc, opts, NULL, error)) {
      DEBUG_PRINT("Failed to upsert document: %s", error->message);
      result = false;
    }
  } else {
    char *str = bson_as_legacy_extended_json(doc, NULL);
    DEBUG_PRINT("Failed to find '_id' in upsert document: %s", str);
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

bool db_upsert_multi_docs(const char *db_name, const char *collection_name, const bson_t *docs, bson_error_t *error) {
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  bson_iter_t iter;
  bool result = true;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  bson_t *opts = BCON_NEW("upsert", BCON_BOOL(true));

  if (bson_iter_init(&iter, docs)) {
    bson_iter_t child;
    while (bson_iter_next(&iter)) {
      bson_t query = BSON_INITIALIZER;
      const uint8_t *data;
      uint32_t len;
      bson_t sub_doc;

      bson_iter_document(&iter, &len, &data);
      bson_init_static(&sub_doc, data, len);

      if (bson_iter_init_find(&child, &sub_doc, "_id")) {
        bson_append_value(&query, "_id", -1, bson_iter_value(&child));
      } else {
        char *str = bson_as_legacy_extended_json(&sub_doc, NULL);
        DEBUG_PRINT("Failed to find '_id' in upsert document: %s", str);
        free(str);

        result = false;
        bson_destroy(&query);
        break;
      }

      if (!mongoc_collection_replace_one(collection, &query, &sub_doc, opts, NULL, error)) {
        DEBUG_PRINT("Failed to upsert document: %s", error->message);
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

bool db_delete_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_error_t *error) {
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  bson_t opts = BSON_INITIALIZER;
  bool result;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  // Delete documents
  result = mongoc_collection_delete_many(collection, query, &opts, NULL, error);
  if (!result) {
    DEBUG_PRINT("Failed to delete documents: %s", error->message);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  // Cleanup
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return true;
}

bool db_drop(const char *db_name, const char *collection_name, bson_error_t *error) {
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  bool result;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  result = mongoc_collection_drop(collection, error);
  if (!result) {
    DEBUG_PRINT("Can't drop %s, error: %s", collection_name, error->message);
  }

  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return result;
}

bool db_count_doc(const char *db_name, const char *collection_name, int64_t *result_count, bson_error_t *error) {
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  int64_t count;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  bson_t *filter = bson_new();  // empty filter

  count = mongoc_collection_count_documents(collection, filter, NULL, NULL, NULL, error);
  if (count < 0) {
    DEBUG_PRINT("Failed to count documents: %s", error->message);
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
    bson_error_t* err)
{
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
  BSON_APPEND_INT64(&opts, "limit", 1);            // we only need one

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