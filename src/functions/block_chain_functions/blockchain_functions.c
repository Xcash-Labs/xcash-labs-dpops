#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "globals.h"
#include "macro_functions.h"
#include "VRF_functions.h"
/*
-----------------------------------------------------------------------------------------------------------
Name: append_string
Description: append string
Parameters:
  data1 - data1
  data2 - data2
Return: Writes the correct code
-----------------------------------------------------------------------------------------------------------
*/

#define append_string(data1,data2,data1_length) \
memcpy(data1+strlen(data1),data2,strnlen(data2,(data1_length) - strlen(data1) - 1));



/*---------------------------------------------------------------------------------------------------------
Name: varint_encode
Description: Encodes varints for the get_block_template blocktemplate_blob
Parameters:
  number - The number to decode
  result - the string to store the result
Return: 1 if successfull, otherwise 0
---------------------------------------------------------------------------------------------------------*/
int varint_encode(long long int number, char *result, const size_t RESULT_TOTAL_LENGTH)
{
  // Variables
  char data[SMALL_BUFFER_SIZE];
  size_t length;
  size_t count = 0;
  size_t count2 = 0;
  int binary_numbers[BITS_IN_BYTE] = {0,0,0,0,0,0,0,0};
  int binary_number_copy;
  long long int number_copy = number;  

  memset(data,0,sizeof(data));
  memset(result,0,RESULT_TOTAL_LENGTH);  

  // check if it should not be encoded
  if (number <= 0xFF)
  {
    snprintf(result,RESULT_TOTAL_LENGTH,"%02llx",number);
    return 1;
  }

  // convert the number to a binary string
  for (count = 0; number_copy != 0; count++)
  {
    if (number_copy % 2 == 1)
    {
      append_string(data,"1",sizeof(data));
    }
    else
    {
      append_string(data,"0",sizeof(data));
    }
    number_copy /= 2; 
  }

  // pad the string to a mulitple of 7 bits  
  for (count = strnlen(data,sizeof(data)); count % (BITS_IN_BYTE-1) != 0; count++)
  {
    append_string(result,"0",RESULT_TOTAL_LENGTH);
  }

  // reverse the string
  length = strnlen(data,sizeof(data));
  for (count = 0; count <= length; count++)
  {
    memcpy(result+strlen(result),&data[length - count],sizeof(char));
  }
  memset(data,0,sizeof(data));
  append_string(data,result,sizeof(data));
  memset(result,0,RESULT_TOTAL_LENGTH);

  /*
  convert each 7 bits to one byte
  set the first bit to 1 for all groups of 7 except for the first group of 7
  */
  length = strnlen(data,sizeof(data)) + (strnlen(data,sizeof(data)) / (BITS_IN_BYTE-1));

 for (count = 0, count2 = 0; count < length; count++)
 {
   if (count % BITS_IN_BYTE == 0 && count != 0)
   {
     // reverse the binary bits
     binary_number_copy = 0;       
     if (((binary_numbers[count2] >> 7) & 1U) == 1) {binary_number_copy |= 1UL << 0;} else {binary_number_copy &= ~(1UL << 0);}
     if (((binary_numbers[count2] >> 6) & 1U) == 1) {binary_number_copy |= 1UL << 1;} else {binary_number_copy &= ~(1UL << 1);}
     if (((binary_numbers[count2] >> 5) & 1U) == 1) {binary_number_copy |= 1UL << 2;} else {binary_number_copy &= ~(1UL << 2);}
     if (((binary_numbers[count2] >> 4) & 1U) == 1) {binary_number_copy |= 1UL << 3;} else {binary_number_copy &= ~(1UL << 3);}
     if (((binary_numbers[count2] >> 3) & 1U) == 1) {binary_number_copy |= 1UL << 4;} else {binary_number_copy &= ~(1UL << 4);}
     if (((binary_numbers[count2] >> 2) & 1U) == 1) {binary_number_copy |= 1UL << 5;} else {binary_number_copy &= ~(1UL << 5);}
     if (((binary_numbers[count2] >> 1) & 1U) == 1) {binary_number_copy |= 1UL << 6;} else {binary_number_copy &= ~(1UL << 6);}
     if (((binary_numbers[count2] >> 0) & 1U) == 1) {binary_number_copy |= 1UL << 7;} else {binary_number_copy &= ~(1UL << 7);}
     binary_numbers[count2] = binary_number_copy;
     count2++;
   } 
   if (count % BITS_IN_BYTE == 0)
   {
     binary_numbers[count2] = count == 0 ? binary_numbers[count2] & ~(1 << (count % BITS_IN_BYTE)) : binary_numbers[count2] | 1 << (count % BITS_IN_BYTE);
   }
   else
   {
     binary_numbers[count2] = strncmp(data + (count - (count2+1)),"1",1) == 0 ? binary_numbers[count2] | 1 << (count % BITS_IN_BYTE) : binary_numbers[count2] & ~(1 << (count % BITS_IN_BYTE));  
   }
 }

  // reverse the last binary_number
  length = strnlen(data,sizeof(data)) / BITS_IN_BYTE;
  binary_number_copy = 0;
  if (((binary_numbers[length] >> 7) & 1U) == 1) {binary_number_copy |= 1UL << 0;} else {binary_number_copy &= ~(1UL << 0);}
  if (((binary_numbers[length] >> 6) & 1U) == 1) {binary_number_copy |= 1UL << 1;} else {binary_number_copy &= ~(1UL << 1);}
  if (((binary_numbers[length] >> 5) & 1U) == 1) {binary_number_copy |= 1UL << 2;} else {binary_number_copy &= ~(1UL << 2);}
  if (((binary_numbers[length] >> 4) & 1U) == 1) {binary_number_copy |= 1UL << 3;} else {binary_number_copy &= ~(1UL << 3);}
  if (((binary_numbers[length] >> 3) & 1U) == 1) {binary_number_copy |= 1UL << 4;} else {binary_number_copy &= ~(1UL << 4);}
  if (((binary_numbers[length] >> 2) & 1U) == 1) {binary_number_copy |= 1UL << 5;} else {binary_number_copy &= ~(1UL << 5);}
  if (((binary_numbers[length] >> 1) & 1U) == 1) {binary_number_copy |= 1UL << 6;} else {binary_number_copy &= ~(1UL << 6);}
  if (((binary_numbers[length] >> 0) & 1U) == 1) {binary_number_copy |= 1UL << 7;} else {binary_number_copy &= ~(1UL << 7);}
  binary_numbers[length] = binary_number_copy;

  // create the varint encoded string
  for (count = 0, count2 = 0; count <= length; count++, count2 += 2)
  {
    snprintf(result+count2,RESULT_TOTAL_LENGTH,"%02x",binary_numbers[length-count] & 0xFF);
  }
  
  return 1;    
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
  int count = 0;
  int counter = 0;
  int bytecount = 0;
  size_t number = 1;
  int start = 0;

  // get the length
  if (varint <= 0xFF)
  {
    return varint;
  }
  else if (varint > 0xFF && varint < 0xFFFF)
  {
    length = 2;
  }
  else if (varint >= 0xFFFF && varint < 0xFFFFFF)
  {
    length = 3;
  }
  else if (varint >= 0xFFFFFF && varint < 0xFFFFFFFF)
  {
    length = 4;
  }
  else if (varint >= 0xFFFFFFFF && varint < 0xFFFFFFFFFF)
  {
    length = 5;
  }
  else if (varint >= 0xFFFFFFFFFF && varint < 0xFFFFFFFFFFFF)
  {
    length = 6;
  }
  else if (varint >= 0xFFFFFFFFFFFF && varint < 0xFFFFFFFFFFFFFF)
  {
    length = 7;
  }
  else
  {
    length = 8;
  }

  // create a byte array for the varint
  char bytes[length];

  for (count = 0; count < length; count++)
  {
    // convert each byte to binary and read the bytes in reverse order
    bytes[count] = ((varint >> (BITS_IN_BYTE * count)) & 0xFF);
  }
    
  for (count = 0, counter = (BITS_IN_BYTE-1), bytecount = 0, start = 0; count < length * BITS_IN_BYTE; count++)
  {
    // loop through each bit until you find the first 1. for every bit after this:
    // if 0 then number = number * 2;
    // if 1 then number = (number * 2) + 1;
    // dont use the bit if its the first bit
    if (counter != (BITS_IN_BYTE-1))
    {
      if (bytes[bytecount] & (1 << counter)) 
      {
        if (start == 1)
        {
          number = (number * 2) + 1;
        }
        start = 1;
      }
      else
      {
        if (start == 1)
        {
          number = number * 2;
        }
      } 
    }
      
    if (counter == 0) 
    {
      counter = (BITS_IN_BYTE-1);
      bytecount++;
    }
    else
    {
      counter--;
    }
  }
 return number;    
}
