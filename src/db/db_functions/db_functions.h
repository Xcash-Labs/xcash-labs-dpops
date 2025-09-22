#ifndef DB_FUNCTIONS_H_   /* Include guard */
#define DB_FUNCTIONS_H_

#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <bson/bson.h>
#include "config.h"
#include "globals.h"
#include "structures.h"
#include "macro_functions.h"
#include "string_functions.h"

int count_documents_in_collection(const char* DATABASE, const char* COLLECTION, const char* DATA);
int count_all_documents_in_collection(const char* DATABASE, const char* COLLECTION);
int insert_document_into_collection_bson(const char* DATABASE, const char* COLLECTION, bson_t* document);
bool delegates_apply_vote_delta(const char* delegate_pubaddr, int64_t delta);
int check_if_database_collection_exist(const char* DATABASE, const char* COLLECTION);
int read_document_field_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, const char* FIELD_NAME, char* result, size_t result_size);
int update_document_from_collection_bson(const char* DATABASE, const char* COLLECTION, const bson_t* filter, const bson_t* update_fields);
int delete_document_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA);
int check_if_database_collection_exist(const char* DATABASE, const char* COLLECTION);
bool is_replica_set_ready(void);
bool add_seed_indexes(void);
bool add_indexes(void);
int count_db_delegates(void);
int count_recs(const bson_t *recs);
bool db_find_all_doc(const char *db_name, const char *collection_name, bson_t *reply, bson_error_t *error);
bool db_find_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_t *reply, bson_error_t *error, bool exclude_id);
bool db_export_collection_to_bson(const char* db_name, const char* collection_name, bson_t* out, bson_error_t* error);
bool db_upsert_multi_docs(const char *db_name, const char *collection_name, const bson_t *docs, bson_error_t *error);
bool db_upsert_doc(const char *db_name, const char *collection_name, const bson_t *doc, bson_error_t *error);
bool db_delete_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_error_t *error);
bool db_drop(const char *db_name, const char *collection_name, bson_error_t *error);
bool db_count_doc(const char *db_name, const char *collection_name, int64_t *result_count, bson_error_t *error);
bool get_vote_total_and_delegate_name(const char* voter_id, int64_t* total_out, char delegate_name_out[MAXIMUM_BUFFER_SIZE_DELEGATES_NAME + 1]);

#endif