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
int check_if_database_collection_exist(const char* DATABASE, const char* COLLECTION);
int read_document_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, char *result);
int read_document_field_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, const char* FIELD_NAME, char* result, size_t result_size);
int database_document_parse_json_data(const char* DATA, struct database_document_fields *result);
int database_multiple_documents_parse_json_data(const char* data, struct database_multiple_documents_fields *result, const int document_count);
int read_document_all_fields_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, struct database_document_fields *result);
int read_multiple_documents_all_fields_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, struct database_multiple_documents_fields *result, const size_t DOCUMENT_COUNT_START, const size_t DOCUMENT_COUNT_TOTAL, const int DOCUMENT_OPTIONS, const char* DOCUMENT_OPTIONS_DATA);
int update_document_from_collection_bson(const char* DATABASE, const char* COLLECTION, const bson_t* filter, const bson_t* update_fields);
int update_multiple_documents_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, const char* FIELD_NAME_AND_DATA);
int update_all_documents_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA);
int delete_document_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA);
int delete_collection_from_database(const char* DATABASE, const char* COLLECTION);
int check_if_database_collection_exist(const char* DATABASE, const char* COLLECTION);
int get_database_data(char *database_data, const char* DATABASE, const char* COLLECTION);
size_t get_database_collection_size(const char* DATABASE, const char* COLLECTION);
int get_data(mongoc_client_t *client, const char *db_name, const char *field_name, char *data);
int get_statistics_totals_by_public_key(const char* public_key, uint64_t* block_verifier_total_rounds, uint64_t* block_verifier_online_total_rounds, 
    uint64_t* block_producer_total_rounds);
bool is_replica_set_ready(void);
bool add_indexes(void);

// from db_helpers
int count_db_delegates(void);
int count_db_statistics(void);
int count_recs(const bson_t *recs);

//from db_operations
bool db_find_all_doc(const char *db_name, const char *collection_name, bson_t *reply, bson_error_t *error);
bool db_find_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_t *reply,
                 bson_error_t *error, bool exclude_id);
bool db_export_collection_to_bson(const char* db_name, const char* collection_name, bson_t* out, bson_error_t* error);
bool db_upsert_multi_docs(const char *db_name, const char *collection_name, const bson_t *docs, bson_error_t *error);
bool db_upsert_doc(const char *db_name, const char *collection_name, const bson_t *doc, bson_error_t *error);
bool db_delete_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_error_t *error);
bool db_drop(const char *db_name, const char *collection_name, bson_error_t *error);
bool db_count_doc(const char *db_name, const char *collection_name, int64_t *result_count, bson_error_t *error);


#endif