/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  krand.h - the game's random number generator            *
*----------------------------------------------------------*
* Measured on an 8MHz 68000 under Hatari, mintlib's rand()  *
* costs 306us -- about 2450 cycles -- and rand()%N costs    *
* 440us, because the modulo is a 32 bit __modsi3 call.  The *
* simulation makes over a hundred of those a frame once     *
* things start exploding, which is 50ms of the 40ms budget  *
* spent entirely on randomness.                             *
*                                                           *
* Koules needs cheap noise, not statistical rigour, so this *
* is a 32 bit xorshift: shifts and xors only, no multiply,  *
* no library call.  KRAND_N() picks a bounded value with a  *
* single mulu.w instead of a division.                      *
*                                                           *
* Both the float reference build and the fixed build use    *
* it, so the numeric harness stays a valid oracle and the   *
* timing comparison isolates the arithmetic rather than the *
* RNG.                                                      *
***********************************************************/

#ifndef _KOULES_KRAND_H
#define _KOULES_KRAND_H

extern unsigned long krand_state;

extern void     ksrand (unsigned long seed);
extern int      krand (void);

/*
 * Uniform value in [0,n) for 0 < n <= 32767, by scaling the 15 bit
 * draw: (r * n) >> 15.  Both operands fit 16 bits, so this is a
 * single mulu.w on a 68000 -- against roughly a thousand cycles
 * for "% n", which is a __modsi3 call.
 */
#define KRAND_N(n)  ((int)(((unsigned long)(unsigned short)krand () \
                            * (unsigned short)(n)) >> 15))

#endif /* _KOULES_KRAND_H */
