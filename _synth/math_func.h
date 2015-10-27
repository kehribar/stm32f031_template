/*-----------------------------------------------------------------------------
/
/
/------------------------------------------------------------------------------
/ <ihsan@kehribar.me>
/----------------------------------------------------------------------------*/
#ifndef _MATH_FUNC_H
#define _MATH_FUNC_H
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "arm_math.h"

typedef union {
  uint16_t value;
  uint8_t bytes[2];
} Word;

typedef union {
  uint32_t value;
  uint16_t words[2];
  uint8_t bytes[4];
} LongWord;

static inline int16_t S16S16MulShift16(int32_t a, int32_t b)
{
  LongWord result;
  result.value = (int32_t)a * (int32_t)b;
  return (int16_t)(result.words[1]);
}

static inline int32_t S16S16MulShift8(int32_t a, int32_t b)
{
  int32_t tmp;

  tmp = a * b;  

  // if(tmp > 8388607)
  // {
  //   return 32767;
  // }
  // else if(tmp < -8388608)
  // {
  //   return -32768;
  // }

  return (int32_t)(tmp >> 8);
}

static inline int32_t S16S16MulShift4(int32_t a, int32_t b)
{
  int32_t tmp;

  tmp = a * b;  

  // if(tmp > 8388607)
  // {
  //   return 32767;
  // }
  // else if(tmp < -8388608)
  // {
  //   return -32768;
  // }

  return (int32_t)(tmp >> 4);
}

#endif _MATH_FUNC_H