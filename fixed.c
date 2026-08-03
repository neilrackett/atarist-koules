/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  fixed.c - 16.16 fixed point helpers                     *
***********************************************************/

#include "fixed.h"

#ifdef SIM_COUNT
extern long     c_normalize, c_fixdiv;
#define CNT(x) (x)++
#else
#define CNT(x) do { } while (0)
#endif

/*
 * floor(sqrt(v)), exact, for a 32 bit unsigned.
 *
 * The obvious restoring form shifts the radicand left two bits an
 * iteration and pulls the top two bits out with "v >> 30".  That
 * is a disaster on a 68000: shifts cost two cycles PER BIT, so a
 * 30 bit shift alone is ~68 cycles and the whole routine measured
 * 3040 cycles.  This form walks a mask downward instead, so every
 * shift is by one or two bits, and the leading-zero skip means
 * small radicands -- which is nearly all of them here, since
 * callers pass dx*dx+dy*dy for a 640x360 arena -- exit early.
 */
ufix_t
isqrt32 (ufix_t v)
{
  ufix_t          root = 0, bit;

  if (v == 0)
    return 0;

  bit = 1UL << 30;
  while (bit > v)
    bit >>= 2;

  while (bit)
    {
      ufix_t          t = root + bit;
      if (v >= t)
	{
	  v -= t;
	  root = (root >> 1) + bit;
	}
      else
	root >>= 1;
      bit >>= 2;
    }
  return root;
}

/*
 * a / b in 16.16, by long division in two steps.
 *
 *   result = (ua << 16) / ub
 *
 * "ua << 16" needs 48 bits, which the 68000 has not got, so the
 * integer and fractional halves are produced separately:
 *
 *   q    = ua / ub                 integer part of a/b
 *   rem  = ua - q*ub               rem < ub
 *   frac = (rem << 16) / ub        16 fractional bits
 *
 * rem << 16 only overflows when ub itself needs more than 16 bits,
 * and in that case both are shifted down by the excess first --
 * rem stays below ub, so all 16 fraction bits survive either way.
 *
 * An earlier single-divide version normalised the divisor into
 * [2^15,2^16) so libgcc could resolve it with one divu.w.  It was
 * faster but it saturated whenever the numerator needed shifting
 * further left than 32 bits allowed -- fixdiv(20, 0.0588) returned
 * 32768 instead of 340.1 -- which is exactly the shape of the
 * gummfactor reciprocal in colisions().  Correctness wins.
 */
fix_t
fixdiv (fix_t a, fix_t b)
{
  ufix_t          ua, ub, q, rem, frac;
  int             neg = 0;
  CNT (c_fixdiv);

  if (a < 0)
    a = -a, neg = 1;
  if (b < 0)
    b = -b, neg ^= 1;
  ua = (ufix_t) a;
  ub = (ufix_t) b;
  if (ub == 0)
    return neg ? -0x7fffffffL : 0x7fffffffL;

  if (ua < ub)
    {
      q = 0;
      rem = ua;			/* the common case: |a| < |b| */
    }
  else
    {
      q = ua / ub;
      rem = ua - q * ub;
      if (q >= 0x8000UL)	/* result would not fit 16.16 */
	return neg ? -0x7fffffffL : 0x7fffffffL;
    }

  /*
   * frac = (rem << 16) / ub, with rem < ub.
   *
   * Normalising the divisor down to 16 bits (k = bitlength(ub)-16,
   * always 0..16) turns this into (rem << (16-k)) / (ub >> k),
   * whose numerator still fits 32 bits and whose quotient still
   * fits 16 -- exactly one divu.w.  Crucially the shift lands on
   * the *divisor*, not the remainder: an earlier version shifted
   * rem down instead and lost the low bits of every small
   * numerator, which is precisely the shape of the gummfactor and
   * gravity terms.
   */
  {
    int             k = 0;
    ufix_t          t = ub >> 16;
    if (t)
      {
	if (t >> 8)
	  k += 8, t >>= 8;
	if (t >> 4)
	  k += 4, t >>= 4;
	if (t >> 2)
	  k += 2, t >>= 2;
	if (t >> 1)
	  k += 1, t >>= 1;
	if (t)
	  k += 1;
      }
    {
      ufix_t          ubn = ub >> k;
      /*
       * ub>>k rounds the divisor DOWN, so the quotient can reach
       * 65536 and overflow divu.w (which would leave the result
       * register untouched, not wrap).  rem < (ubn<<k) is exactly
       * the condition that keeps it inside 16 bits.
       */
      if (rem >= (ubn << k))
	frac = 0xffffUL;
      else
	frac = udiv16 (rem << (16 - k), (unsigned short) ubn);
    }
  }

  if (frac > 0xffffUL)
    frac = 0xffffUL;		/* rounding at the boundary */

  q = (q << 16) | frac;
  return neg ? -(fix_t) q : (fix_t) q;
}

/*
 * Scale (*x,*y) so that its length becomes "size".
 *
 * Upstream:
 *   len = sqrt(x*x + y*y);  if (!len) len = 1;
 *   *x *= size/len;  *y *= size/len;
 *
 * x and y can be as large as the playfield diagonal, so x*x in
 * 16.16 overflows badly.  Both are first shifted right by the
 * smallest amount that keeps each below 2^15 (so the sum of
 * squares stays inside 31 bits); the ratio x/len is unaffected
 * by a shift applied to both.  The unit vector is then formed in
 * 1.15, which is always bounded by 1.0 no matter how short the
 * input vector is -- the reason for two divides here rather than
 * one divide by the length, which would overflow for near-zero
 * vectors (two objects sitting on top of each other, which does
 * happen inside colisions()).
 */
void
fix_normalize (fix_t * px, fix_t * py, fix_t size)
{
  fix_t           x = *px, y = *py;
  fix_t           xr, yr;
  CNT (c_normalize);
  ufix_t          ax, ay, m, t, len;
  int             sh = 0;

  ax = (ufix_t) (x < 0 ? -x : x);
  ay = (ufix_t) (y < 0 ? -y : y);
  m = ax | ay;
  if (m == 0)
    {
      *px = 0;
      *py = 0;
      return;
    }

  t = m >> 15;
  if (t)
    {
      if (t >> 8)
	sh += 8, t >>= 8;
      if (t >> 4)
	sh += 4, t >>= 4;
      if (t >> 2)
	sh += 2, t >>= 2;
      if (t >> 1)
	sh += 1, t >>= 1;
      if (t)
	sh += 1;
    }

  xr = x >> sh;
  yr = y >> sh;
  len = isqrt32 ((ufix_t) (xr * xr + yr * yr));
  if (len == 0)
    {
      *px = 0;
      *py = 0;
      return;
    }

  /* |xr| <= len so the quotient never exceeds 1.0 in 1.15 */
  /*
   * Unit vector in 1.15, then scale.  |xr| <= len and len fits 16
   * bits (xr,yr are both under 2^15, so len < 46341), and the
   * quotient is at most 32768 -- exactly the preconditions for a
   * single divu.w.  Signs are handled outside the divide because
   * divs.w would overflow at quotient 32768.
   */
  {
    ufix_t          axr = (ufix_t) (xr < 0 ? -xr : xr);
    ufix_t          ayr = (ufix_t) (yr < 0 ? -yr : yr);
    fix_t           ux = (fix_t) (ufix_t) udiv16 (axr << 15, (unsigned short) len);
    fix_t           uy = (fix_t) (ufix_t) udiv16 (ayr << 15, (unsigned short) len);
    if (xr < 0)
      ux = -ux;
    if (yr < 0)
      uy = -uy;
    *px = fixmul ((fix_t) ((ufix_t) ux << 1), size);
    *py = fixmul ((fix_t) ((ufix_t) uy << 1), size);
  }
}
