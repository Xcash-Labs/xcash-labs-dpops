#ifndef BLOCKCHAIN_FUNCTIONS_H_   /* Include guard */
#define BLOCKCHAIN_FUNCTIONS_H_

int varint_encode(long long int number, char *result, const size_t RESULT_TOTAL_LENGTH);
size_t varint_decode(size_t varint);

#endif