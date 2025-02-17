#ifndef VARIABLES_H_   /* Include guard */
#define VARIABLES_H_

#include <mongoc/mongoc.h>

// database
extern mongoc_client_pool_t* database_client_thread_pool;

#endif