#ifndef SHA256_EL_H
#define SHA256_EL_H

#include <stdint.h>
#include <stddef.h>

#define SHA256_EL_HASH_SIZE 32

void sha256EL(const uint8_t *data, size_t len, uint8_t *out_hash);

#endif