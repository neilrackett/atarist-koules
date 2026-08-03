/***********************************************************
*  krand.c - see krand.h for why this exists               *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include "krand.h"

unsigned long   krand_state = 2463534242UL;

void
ksrand (unsigned long seed)
{
  krand_state = seed ? seed : 2463534242UL;
}

/*
 * Marsaglia xorshift32.  Returns 15 bits so it drops into every
 * existing rand() call site unchanged (upstream only ever uses
 * rand() modulo something small, or as a raw magnitude).
 */
int
krand (void)
{
  unsigned long   x = krand_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  krand_state = x;
  return (int) ((x >> 8) & 0x7fffUL);
}
