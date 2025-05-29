#include "blockchain_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: varint_encode
Description: Encodes varints for the get_block_template blocktemplate_blob
Parameters:
  number - The number to decode
  result - the string to store the result
Return: 1 if successfull, otherwise 0
---------------------------------------------------------------------------------------------------------*/
int varint_encode(long long int number, char *result, const size_t RESULT_TOTAL_LENGTH) {
  size_t index = 0;
  unsigned char byte_array[16];  // Enough space for 64-bit integer varint encoding

  if (number < 0) {
      return XCASH_ERROR; // VarInt encoding typically does not support negative numbers
  }

  // Encode number into byte_array
  while (number >= 0x80) {
      byte_array[index++] = (number & 0x7F) | 0x80;
      number >>= 7;
  }
  byte_array[index++] = number & 0x7F;  // Last byte without continuation bit

  // Convert bytes to hex string
  if (RESULT_TOTAL_LENGTH < index * 2 + 1) {
      return XCASH_ERROR;  // Result buffer too small
  }

  for (size_t i = 0; i < index; i++) {
      snprintf(result + (i * 2), RESULT_TOTAL_LENGTH - (i * 2), "%02x", byte_array[i]);
  }

  result[index * 2] = '\0';  // Null-terminate

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: varint_decode
Description: Decodes varints for the get_block_template blocktemplate_blob
Parameters:
  varint - The varint to decode
Return: The decoded varint
---------------------------------------------------------------------------------------------------------*/
size_t varint_decode(size_t varint)
{
  // Variables
  int length = 0;
  size_t number = 1;
  int byte_index = 0;
  int bit_index = BITS_IN_BYTE - 1;
  int start = 0;

  // Determine length based on varint size
  if (varint <= 0xFF) {
    return varint;
  } else if (varint <= 0xFFFF) {
    length = 2;
  } else if (varint <= 0xFFFFFF) {
    length = 3;
  } else if (varint <= 0xFFFFFFFF) {
    length = 4;
  } else if (varint <= 0xFFFFFFFFFF) {
    length = 5;
  } else if (varint <= 0xFFFFFFFFFFFF) {
    length = 6;
  } else if (varint <= 0xFFFFFFFFFFFFFF) {
    length = 7;
  } else {
    length = 8;
  }

  // Extract bytes (little endian)
  unsigned char bytes[8] = {0};
  for (int i = 0; i < length; i++) {
    bytes[i] = (varint >> (BITS_IN_BYTE * i)) & 0xFF;
  }

  // Decode bits
  for (int i = 0; i < length * BITS_IN_BYTE; i++) {
    if (bit_index != (BITS_IN_BYTE - 1)) {
      if (bytes[byte_index] & (1 << bit_index)) {
        if (start) {
          number = (number << 1) | 1;
        }
        start = 1;
      } else {
        if (start) {
          number <<= 1;
        }
      }
    }

    if (--bit_index < 0) {
      bit_index = BITS_IN_BYTE - 1;
      byte_index++;
    }
  }

  return number;
}