#ifndef DB_FUNCTIONS_H_   /* Include guard */
#define DB_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>


#include "config.h"
#include "globals.h"
#include "macro_functions.h"


//#include "database_functions.h"
//#include "network_functions.h"
//#include "string_functions.h"
//#include "vrf.h"
//#include "crypto_vrf.h"
//#include "VRF_functions.h"
//#include "sha512EL.h"

int count_documents_in_collection(const char* DATABASE, const char* COLLECTION, const char* DATA);
int count_all_documents_in_collection(const char* DATABASE, const char* COLLECTION);
#endif