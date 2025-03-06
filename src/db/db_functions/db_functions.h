#ifndef DB_FUNCTIONS_H_   /* Include guard */
#define DB_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>

#include "config.h"
#include "globals.h"
#include "structures.h"
#include "macro_functions.h"
#include "string_functions.h"
#include "cached_hashes.h"

//#include "database_functions.h"
//#include "network_functions.h"

//#include "vrf.h"
//#include "crypto_vrf.h"
//#include "VRF_functions.h"
//#include "sha512EL.h"

int count_documents_in_collection(const char* DATABASE, const char* COLLECTION, const char* DATA);
int count_all_documents_in_collection(const char* DATABASE, const char* COLLECTION);
int insert_document_into_collection_json(const char* DATABASE, const char* COLLECTION, const char* DATA);
int check_if_database_collection_exist(const char* DATABASE, const char* COLLECTION);
int read_document_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, char *result);
int read_document_field_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, const char* FIELD_NAME, char *result);
int database_document_parse_json_data(const char* DATA, struct database_document_fields *result);
int database_multiple_documents_parse_json_data(const char* data, struct database_multiple_documents_fields *result, const int document_count);
int read_document_all_fields_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, struct database_document_fields *result);
int read_multiple_documents_all_fields_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, struct database_multiple_documents_fields *result, const size_t DOCUMENT_COUNT_START, const size_t DOCUMENT_COUNT_TOTAL, const int DOCUMENT_OPTIONS, const char* DOCUMENT_OPTIONS_DATA);
int update_document_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, const char* FIELD_NAME_AND_DATA);
int update_multiple_documents_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, const char* FIELD_NAME_AND_DATA);
int update_all_documents_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA);
int delete_document_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA);
int delete_collection_from_database(const char* DATABASE, const char* COLLECTION);
int delete_database(const char* DATABASE);
int check_if_database_collection_exist(const char* DATABASE, const char* COLLECTION);
int get_database_data(char *database_data, const char* DATABASE, const char* COLLECTION);
int get_database_data_hash(char *data_hash, const char* DATABASE, const char* COLLECTION);
size_t get_database_collection_size(const char* DATABASE, const char* COLLECTION);
//void reserve_proofs_delegate_check(void);

#endif