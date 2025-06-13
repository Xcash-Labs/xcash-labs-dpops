#ifndef __DB_OPERATIONS_H_
#define __DB_OPERATIONS_H_

#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include "cached_hashes.h"
#include "config.h"
#include "globals.h"
#include "macro_functions.h"

bool db_find_all_doc(const char *db_name, const char *collection_name, bson_t *reply, bson_error_t *error);
bool db_find_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_t *reply,
                 bson_error_t *error, bool exclude_id);
bool db_export_collection_to_bson(const char* db_name, const char* collection_name, bson_t* out, bson_error_t* error);
bool db_upsert_multi_docs(const char *db_name, const char *collection_name, const bson_t *docs, bson_error_t *error);
bool db_upsert_doc(const char *db_name, const char *collection_name, const bson_t *doc, bson_error_t *error);
bool db_delete_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_error_t *error);
bool db_drop(const char *db_name, const char *collection_name, bson_error_t *error);
bool db_count_doc(const char *db_name, const char *collection_name, int64_t *result_count, bson_error_t *error);
bool db_count_doc_by(const char *db_name, const char *collection_name, const bson_t *query, int64_t *result_count, bson_error_t *error);

bool get_db_data_hash(const char *collection_prefix, char *db_hash_result);

#endif