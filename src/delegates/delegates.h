#ifndef __XCASH_DELEGATES_H
#define __XCASH_DELEGATES_H

#include "db_operations.h"
#include "xcash_db_helpers.h"
#include "globals.h"

int read_organize_delegates(delegates_t* delegates, size_t* delegates_count_result);

#endif