/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  fixed.h - 16.16 fixed point for the Atari ST port       *
*----------------------------------------------------------*
* The 68000 has no FPU and gcc's soft float costs several  *
* hundred cycles per operation, so the whole simulation is *
* carried in 16.16 fixed point.  Range is +/-32767 with a  *
* resolution of 1/65536; the playfield is 640x360 logical  *
* units so there is ~50x headroom on positions.            *
*                                                          *
* Rules of the road:                                       *
*   - add/sub/compare are plain 32-bit integer ops         *
*   - fixmul() is four mulu.w, no libgcc call              *
*   - fixdiv() is the expensive one; avoid it in loops     *
*   - squared-distance tests are done in *integer* units   *
*     (see FIX2I) because dx*dx in 16.16 overflows and     *
*     because upstream truncated those to int anyway       *
***********************************************************/

#ifndef _KOULES_FIXED_H
#define _KOULES_FIXED_H

/*
 * Must be exactly 32 bits.  "long" would be 64 bit on the LP64
 * host, and then the host harness would silently model arithmetic
 * the 68000 cannot do -- overflow and saturation behaviour has to
 * match the target or the oracle is worthless.
 */
#include <stdint.h>
typedef int32_t  fix_t;
typedef uint32_t ufix_t;

#define FIXSH   16
#define FIXONE  65536L
#define FIXHALF 32768L
#define FIXMASK 0xffffL

/* Compile-time literal.  The double maths is folded by the
   compiler; nothing floating point survives into the object. */
#define FIX(f)   ((fix_t)((f) * 65536.0 + ((f) < 0 ? -0.5 : 0.5)))

#define FIXI(i)  ((fix_t)((ufix_t)(i) << FIXSH))

/* fix -> int.  NB this floors, where a C float->int cast would
   truncate toward zero.  Positions are non-negative in play so
   the two agree; the difference only shows up for objects that
   have already left the arena and are about to be destroyed. */
#define FIX2I(x) ((int)((x) >> FIXSH))

#define FIXABS(x) ((x) < 0 ? -(x) : (x))

/*
 * a*b >> 16, via four 16x16->32 unsigned multiplies.
 * gcc emits mulu.w for each of these on a plain 68000; the
 * "long long" form would call __muldi3 instead.
 */
static __inline__ fix_t
fixmul (fix_t a, fix_t b)
{
  ufix_t          ua, ub, r;
  unsigned short  ah, al, bh, bl;
  int             neg = 0;

  if (a < 0)
    a = -a, neg = 1;
  if (b < 0)
    b = -b, neg ^= 1;
  ua = (ufix_t) a;
  ub = (ufix_t) b;
  ah = (unsigned short) (ua >> 16);
  al = (unsigned short) ua;
  bh = (unsigned short) (ub >> 16);
  bl = (unsigned short) ub;

  r = (((ufix_t) ah * bh) << 16)
    + (ufix_t) ah *bl
    + (ufix_t) al *bh
    + ((((ufix_t) al * bl) >> 16));

  return neg ? -(fix_t) r : (fix_t) r;
}

/*
 * Same, for a multiplier known to be a proper fraction
 * (0 <= b < 1.0) -- SLOWDOWN and friends.  Two multiplies.
 */
static __inline__ fix_t
fixmulf (fix_t a, fix_t b)
{
  ufix_t          ua, r;
  unsigned short  ah, al, bl;
  int             neg = 0;

  if (a < 0)
    a = -a, neg = 1;
  ua = (ufix_t) a;
  ah = (unsigned short) (ua >> 16);
  al = (unsigned short) ua;
  bl = (unsigned short) b;

  r = (ufix_t) ah *bl + ((((ufix_t) al * bl) >> 16));

  return neg ? -(fix_t) r : (fix_t) r;
}

extern fix_t    fixdiv (fix_t a, fix_t b);

/*
 * 32/16 -> 16 unsigned divide.  The 68000 has exactly this
 * instruction, but gcc cannot prove the quotient fits 16 bits and
 * so calls __udivsi3, which is an order of magnitude dearer.  Only
 * use where the caller has established that num < 2^31, den fits
 * 16 bits, and num/den < 2^16 -- fix_normalize() is the case that
 * matters, and it guarantees all three by construction.
 */
#ifdef __m68k__
static __inline__ unsigned short
udiv16 (unsigned long num, unsigned short den)
{
  __asm__ ("divu.w %1,%0" : "+d" (num) : "dmi" (den));
  return (unsigned short) num;
}
#else
#define udiv16(num,den) ((unsigned short)((num) / (unsigned long)(den)))
#endif

/* isqrt of a 32 bit unsigned; exact floor(sqrt(v)). */
extern ufix_t   isqrt32 (ufix_t v);

/* Scale (*x,*y) to have length "size".  Replaces the float
   normalize() and its sqrt() call. */
extern void     fix_normalize (fix_t * x, fix_t * y, fix_t size);

/* Length of an integer vector -- floor(sqrt(dx*dx+dy*dy)).
   Upstream assigned every sqrt() result here to an int, so this
   is faithful, not an approximation. */
#define IDIST(dx,dy) ((int) isqrt32 ((ufix_t) ((fix_t)(dx) * (dx) + (fix_t)(dy) * (dy))))

/*
 * Angles are integer degrees.  Upstream's RAD(n) turned degrees
 * into radians for libm; here it is the identity and the table
 * is indexed directly, so ROTSTEP stays exactly 10 degrees and a
 * rocket still has exactly 36 headings.
 */
#define FIXANG_BIAS 360
extern const short fix_sin_tab[];       /* 1.15, index = deg + 360 */

/* Valid for deg in [-360, +629], which covers rotation +/- 30
   and rotation + (rand()%45 - 22). */
#define fixsin(deg) ((fix_t)((ufix_t)fix_sin_tab[(deg) + FIXANG_BIAS] << 1))
#define fixcos(deg) ((fix_t)((ufix_t)fix_sin_tab[(deg) + FIXANG_BIAS + 90] << 1))

#endif /* _KOULES_FIXED_H */
