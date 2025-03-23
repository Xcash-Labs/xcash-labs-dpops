#ifndef XCASH_BLOCK_H
#define XCASH_BLOCK_H

#include <stdlib.h>
#include <stdbool.h>
#include <jansson.h>
#include "config.h"
#include "macro_functions.h"
#include "uv_net_multi.h"
#include "xcash_net.h"
#include "xcash_round.h"
#include "variables.h"

bool sync_block_producers(void);

#endif
