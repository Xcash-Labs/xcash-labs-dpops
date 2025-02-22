#include "db_init.h"

bool initialize_database(void){
    return initialize_mongo_database(DATABASE_CONNECTION, &database_client_thread_pool);
}

void shutdown_database(void){
    shutdown_mongo_database(&database_client_thread_pool);
}

bool initialize_mongo_database(const char *mongo_uri, mongoc_client_pool_t **db_client_thread_pool) {
    mongoc_uri_t *uri_thread_pool;
    bson_error_t error;
    // Initialize the MongoDB client library
    mongoc_init();
    // Create a new URI object from the provided URI string
    uri_thread_pool = mongoc_uri_new_with_error(mongo_uri, &error);
    if (!uri_thread_pool) {
        ERROR_PRINT("Failed to parse URI: %s\nError message: %s", mongo_uri, error.message);
        return XCASH_ERROR;
    }
    // Create a new client pool with the parsed URI object
    *db_client_thread_pool = mongoc_client_pool_new(uri_thread_pool);
    if (!*db_client_thread_pool) {
        ERROR_PRINT("Failed to create a new client pool.");
        mongoc_uri_destroy(uri_thread_pool);
        return XCASH_ERROR;
    }
    mongoc_uri_destroy(uri_thread_pool);
    return XCASH_OK;
}

void shutdown_mongo_database(mongoc_client_pool_t **db_client_thread_pool) {
    if (*db_client_thread_pool) {
        mongoc_client_pool_destroy(*db_client_thread_pool);
        *db_client_thread_pool = NULL;
    }
    mongoc_cleanup();
}

bool initialize_network_nodes(void)
{
    // Check if the collection is empty
    int document_count = count_all_documents_in_collection(DATABASE_NAME, "delegates");

    if (document_count > 0)
    {
        DEBUG_PRINT("Network nodes collection already populated, skipping initialization.");
        return XCASH_OK;
    }

    // Insert all network nodes from the array
    size_t i = 0;
    while (network_nodes[i].public_address != NULL && strlen(network_nodes[i].public_address) > 0)
    {

        char json_data[SMALL_BUFFER_SIZE];
        snprintf(json_data, sizeof(json_data),
                 "{ \"public_address\": \"%s\","
                 " \"total_vote_count\": \"0\","
                 " \"IP_address\": \"%s\","
                 " \"delegate_name\": \"%s\","
                 " \"about\": \"Official X-Labs node\","
                 " \"website\": \"\","
                 " \"team\": \"X-Labs Team\","
                 " \"shared_delegate_status\": \"solo\","
                 " \"delegate_fee\": \"\","
                 " \"server_specs\": \"Operating System = Ubuntu 22.04\","
                 " \"block_verifier_score\": \"0\","
                 " \"online_status\": \"true\","
                 " \"block_verifier_total_rounds\": \"0\","
                 " \"block_verifier_online_total_rounds\": \"0\","
                 " \"block_verifier_online_percentage\": \"0\","
                 " \"block_producer_total_rounds\": \"0\","
                 " \"block_producer_block_heights\": \"\","
                 " \"public_key\": \"%s\" }",
                 network_nodes[i].public_address,
                 network_nodes[i].ip_address,
                 network_nodes[i].delegate_name,
                 network_nodes[i].public_key);

        if (strlen(json_data) >= SMALL_BUFFER_SIZE - 1)
        {
            ERROR_PRINT("JSON data too large for buffer, skipping node: %s", network_nodes[i].public_address);
            i++; // Move to next node
            continue;
        }

        // Insert into database
        if (insert_document_into_collection_json(DATABASE_NAME, "delegates", json_data) != 1)
        {
            ERROR_PRINT("Failed to add network node: %s", network_nodes[i].public_address);
            return XCASH_ERROR; // Stop immediately if any insertion fails
        }
        DEBUG_PRINT("Added network node: %s", network_nodes[i].ip_address);
        i++;
    }

    return XCASH_OK;
}