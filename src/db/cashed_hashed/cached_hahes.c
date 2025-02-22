# include "cached_hashes.h"


/*---------------------------------------------------------------------------------------------------------
Name: insert_document_into_collection_json
Description: Inserts a document into the collection in the database from json data
Parameters:
  DATABASE - The database name
  COLLECTION - The collection name
  DATA - The json data to insert into the collection
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int del_hash(mongoc_client_t *client, const char *db_name) {
    if (!client || !db_name) {
        PRINT_ERROR("del_hash() received NULL parameters.\n");
        return -1;
    }

    mongoc_collection_t *collection = mongoc_client_get_collection(client, db_name, "hashes");
    if (!collection) {
        PRINT_ERROR("Failed to get collection 'hashes' for database: %s\n", db_name);
        return -1;
    }

    bson_t *filter = BCON_NEW("db_name", BCON_UTF8(db_name));
    if (!filter) {
        PRINT_ERROR("Failed to create BSON filter for db_name: %s\n", db_name);
        mongoc_collection_destroy(collection);
        return -1;
    }

    bson_error_t error;
    int result = 0;

    if (mongoc_collection_delete_one(collection, filter, NULL, NULL, &error)) {
        PRINT_DEBUG("Hash successfully deleted for %s\n", db_name);
        result = XCASH_OK;
    } else {
        PRINT_ERROR("Delete hashes %s failed: %s\n", db_name, error.message);
        result = -1;
    }

    bson_destroy(filter);
    mongoc_collection_destroy(collection);
    
    return result;
}