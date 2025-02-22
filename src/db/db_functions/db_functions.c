include "db_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: count_documents_in_collection
Description: Counts the documents in the collection that match a specific field name and field
Parameters:
  DATABASE - The database name
  COLLECTION - The collection name
  DATA - The json data to use to search the collection for
Return: -1 if an error has occured, otherwise the amount of documents that match a specific field name and field in the collection
---------------------------------------------------------------------------------------------------------*/

int count_all_documents_in_collection(const char* DATABASE, const char* COLLECTION) {
  // Sanity check to ensure database is initialized
  if (!database_client_thread_pool) {
      DEBUG_PRINT("Database client pool is not initialized! Cannot count documents.");
      return -1;
  }

  // Get a temporary database connection
  mongoc_client_t* database_client_thread = mongoc_client_pool_pop(database_client_thread_pool);
  if (!database_client_thread) {
      DEBUG_PRINT("Failed to get a database connection from the pool.");
      return -1;
  }

  // Get the collection
  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!collection) {
      DEBUG_PRINT("Failed to get collection: %s", COLLECTION);
      mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
      return -1;
  }

  // Check if the collection exists
  if (check_if_database_collection_exist(DATABASE, COLLECTION) == 0) {
      DEBUG_PRINT("Collection does not exist: %s", COLLECTION);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
      return 0; // Collection does not exist → Return 0 instead of an error
  }

  // Count the documents
  bson_error_t error;
  int count = (int)mongoc_collection_count_documents(collection, NULL, NULL, NULL, NULL, &error);
  if (count < 0) {
      ERROR_PRINT("Error counting documents in %s: %s", COLLECTION, error.message);
  }

  // Cleanup
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
  
  return count;
}