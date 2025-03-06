#ifndef CACHED_HASHES_H_
#define CACHED_HASHES_H_

//#include <stdlib.h>
//#include <string.h>
//#include <stdio.h>
//#include <sys/time.h>
//#include <openssl/md5.h>

#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"

 // For MD5 functions

int del_hash(mongoc_client_t *client, const char *db_name);

int get_multi_hash(mongoc_client_t *client, const char *db_prefix, char *hash);
//int drop_all_hashes(mongoc_client_t *client);

//void bin_to_hex(const unsigned char *bin_data, int data_size, char *buf);

//void md5_hex(const char * src, char * dest);


#endif
