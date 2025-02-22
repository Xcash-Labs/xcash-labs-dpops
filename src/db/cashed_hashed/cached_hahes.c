# include "cached_hashes.h"

/*---------------------------------------------------------------------------------------------------------
Name: del_hash
Description: Deletes a hash entry from the "hashes" collection in the specified database.
             If the collection does not exist, the function returns 0 without attempting deletion.
Parameters:
  client   - A pointer to the MongoDB client connection.
  db_name  - The name of the database where the "hashes" collection is located.
Return: 
  -1 if an error occurs (e.g., database/collection access failure, deletion error).
   0 if the collection does not exist or no matching document was found.
   1 if a hash was successfully deleted.
---------------------------------------------------------------------------------------------------------*/
int del_hash(mongoc_client_t *client, const char *db_name) {
    if (!client || !db_name) {
        ERROR_PRINT("del_hash() received NULL parameters.");
        return -1;
    }

    mongoc_collection_t *collection = mongoc_client_get_collection(client, db_name, "hashes");
    if (!collection) {
        ERROR_PRINT("Failed to get collection 'hashes' for database: %s", db_name);
        return -1;
    }

    bson_t *filter = BCON_NEW("db_name", BCON_UTF8(db_name));
    if (!filter) {
        ERROR_PRINT("Failed to create BSON filter for db_name: %s", db_name);
        mongoc_collection_destroy(collection);
        return -1;
    }

    bson_error_t error;
    int result = 0;

    if (mongoc_collection_delete_one(collection, filter, NULL, NULL, &error)) {
        DEBUG_PRINT("Hash successfully deleted for %s", db_name);
        result = XCASH_OK;
    } else {
        ERROR_PRINT("Delete hashes %s failed: %s", db_name, error.message);
        result = -1;
    }

    bson_destroy(filter);
    mongoc_collection_destroy(collection);
    
    return result;
}