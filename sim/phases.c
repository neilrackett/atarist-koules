/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  sim/phases.c - where the frame time actually goes       *
*                                                          *
*  BENCHX/BENCHF report the frame cost.  This one splits   *
*  it up, because a total is not actionable: it times each *
*  phase of the frame cumulatively, then micro-benchmarks  *
*  the three things the host profile fingered as suspects  *
*  -- rand()%N (a 32 bit __modsi3 on a 68000), the         *
*  explosion fragment loop, and the creator cloud's        *
*  rejection sampler.                                      *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include <stdio.h>
#include <stdlib.h>
#include "koules.h"
#include "physics.h"
#include "scenario.h"
#ifdef KOULES_FLOAT
#include <math.h>
/* physics_float.c's normalize() is gnu89-inline so it has no
   external symbol; this is the same body, for timing only */
static void
ref_normalize (float *x, float *y, float size)
{
  float           length = sqrt ((*x) * (*x) + (*y) * (*y));
  if (length == 0)
    length = 1;
  *x *= size / length;
  *y *= size / length;
}
#endif

#ifdef __m68k__
#include <osbind.h>
#include <mint/sysvars.h>
#define TICKS()  (*_hz_200)
#else
#include <sys/time.h>
static unsigned long
TICKS (void)
{
  struct timeval  tv;
  gettimeofday (&tv, 0);
  return (unsigned long) (tv.tv_sec * 200 + tv.tv_usec / 5000);
}
#endif

#ifdef KOULES_FLOAT
#define BUILD "float"
#else
#define BUILD "fixed"
#endif

#define FRAMES 400
#ifndef N
#define N      20
#endif

/* phase mask */
#define P_VALUES 1
#define P_FORCES 2
#define P_COLIS  4
#define P_MOVE   8
#define P_ACCEL  16

static void
frame (int f, int mask)
{
  int             r;
  if (mask & P_ACCEL)
    for (r = 0; r < nrockets; r++)
      if (object[r].live && object[r].type == ROCKET)
	{
	  object[r].rotation += ROTSTEP;
#ifndef KOULES_FLOAT
	  ANGWRAP (object[r].rotation);
#else
	  while (object[r].rotation >= RAD (360))
	    object[r].rotation -= RAD (360);
#endif
	  if ((f & 3) != 3)
	    accel (r, OVAL (1.0));
	}
  if (mask & P_VALUES)
    update_values ();
  if (mask & P_FORCES)
    update_forces ();
  if (mask & P_COLIS)
    colisions ();
  if (mask & P_MOVE)
    {
      move_objects ();
      check_limit ();
    }
}

static long
run (int mask)
{
  unsigned long   t0, t1;
  int             f;
  sim_setup (N, 12345);
  t0 = TICKS ();
  for (f = 0; f < FRAMES; f++)
    {
      frame (f, mask);
      sim_respawn ();
    }
  t1 = TICKS ();
  return (long) (t1 - t0) * 5L;
}

static void
show (CONST char *what, long ms)
{
  printf ("  %-34s %4ld.%02ld ms/frame\n", what,
	  ms / FRAMES, (ms % FRAMES) * 100 / FRAMES);
}

int
main (void)
{
  long            base, t;
  unsigned long   t0, t1;
  int             i;
  volatile int    sink = 0;

#ifdef __m68k__
  Super (0L);
#endif

  printf ("PHASES-START %s  (n=%d, %d frames)\n", BUILD, N, FRAMES);

  base = run (0);		/* loop + respawn overhead only */
  show ("loop + respawn only", base);
  t = run (P_VALUES);
  show ("+ update_values", t - base);
  base = t;
  t = run (P_VALUES | P_FORCES);
  show ("+ update_forces", t - base);
  base = t;
  t = run (P_VALUES | P_FORCES | P_COLIS);
  show ("+ colisions", t - base);
  base = t;
  t = run (P_VALUES | P_FORCES | P_COLIS | P_MOVE);
  show ("+ move_objects + check_limit", t - base);
  base = t;
  t = run (P_VALUES | P_FORCES | P_COLIS | P_MOVE | P_ACCEL);
  show ("+ accel (thrust, sin/cos, exhaust)", t - base);
  printf ("  full frame                          %4ld.%02ld ms/frame\n",
	  t / FRAMES, (t % FRAMES) * 100 / FRAMES);

  /* --- micro benchmarks --- */
  printf ("micro:\n");

  t0 = TICKS ();
  for (i = 0; i < 20000; i++)
    sink += rand () % 3096;
  t1 = TICKS ();
  printf ("  rand()%%3096                         %ld us each\n",
	  (long) (t1 - t0) * 5000L / 20000L);

  t0 = TICKS ();
  for (i = 0; i < 20000; i++)
    sink += rand ();
  t1 = TICKS ();
  printf ("  rand() alone                        %ld us each\n",
	  (long) (t1 - t0) * 5000L / 20000L);

  sim_setup (N, 12345);
  t0 = TICKS ();
  for (i = 0; i < 200; i++)
    explosion (320, 180, BBALL, ' ', 0);
  t1 = TICKS ();
  printf ("  explosion(BBALL, 202 fragments)     %ld.%ld ms each\n",
	  (long) (t1 - t0) * 5L / 200L, ((long) (t1 - t0) * 50L / 200L) % 10);

  sim_setup (N, 12345);
  t0 = TICKS ();
  for (i = 0; i < 100; i++)
    creators_points (ROCKET_RADIUS, 320, 180, 96);
  t1 = TICKS ();
  printf ("  creators_points(rocket, 153 parts)  %ld.%ld ms each\n",
	  (long) (t1 - t0) * 5L / 100L, ((long) (t1 - t0) * 50L / 100L) % 10);

#ifndef KOULES_FLOAT
  {
    fix_t           a = FIX (123.456), b = FIX (7.89), r = 0;
    t0 = TICKS ();
    for (i = 0; i < 20000; i++)
      r += fixmul (a + i, b);
    t1 = TICKS ();
    printf ("  fixmul                              %ld us each\n",
	    (long) (t1 - t0) * 5000L / 20000L);
    t0 = TICKS ();
    for (i = 0; i < 20000; i++)
      r += fixdiv (a + i, b);
    t1 = TICKS ();
    printf ("  fixdiv                              %ld us each\n",
	    (long) (t1 - t0) * 5000L / 20000L);
    t0 = TICKS ();
    for (i = 0; i < 20000; i++)
      r += (fix_t) isqrt32 ((ufix_t) (i * 37 + 1));
    t1 = TICKS ();
    printf ("  isqrt32                             %ld us each\n",
	    (long) (t1 - t0) * 5000L / 20000L);
    t0 = TICKS ();
    for (i = 0; i < 20000; i++)
      {
	fix_t           x = FIX (300) + i, y = FIX (-170) - i;
	fix_normalize (&x, &y, FIX (1.2));
	r += x + y;
      }
    t1 = TICKS ();
    printf ("  fix_normalize                       %ld us each\n",
	    (long) (t1 - t0) * 5000L / 20000L);
    sink += (int) r;
  }
#else
  {
    float           a = 123.456f, b = 7.89f, r = 0;
    t0 = TICKS ();
    for (i = 0; i < 20000; i++)
      r += (a + i) * b;
    t1 = TICKS ();
    printf ("  float multiply                      %ld us each\n",
	    (long) (t1 - t0) * 5000L / 20000L);
    t0 = TICKS ();
    for (i = 0; i < 20000; i++)
      r += (a + i) / b;
    t1 = TICKS ();
    printf ("  float divide                        %ld us each\n",
	    (long) (t1 - t0) * 5000L / 20000L);
    t0 = TICKS ();
    for (i = 0; i < 2000; i++)
      {
	float           x = 300.0f + i, y = -170.0f - i;
	ref_normalize (&x, &y, 1.2f);
	r += x + y;
      }
    t1 = TICKS ();
    printf ("  normalize (sqrt + 2 div)            %ld us each\n",
	    (long) (t1 - t0) * 5000L / 2000L);
    sink += (int) r;
  }
#endif

  t0 = TICKS ();
  for (i = 0; i < 400; i++)
    points1 ();
  t1 = TICKS ();
  printf ("  points1() over MAXPOINT=%d        %ld.%02ld ms each\n", MAXPOINT,
	  (long) (t1 - t0) * 5L / 400L, ((long) (t1 - t0) * 500L / 400L) % 100);

  printf ("sink %d\n", sink);
  printf ("PHASES-DONE %s\n", BUILD);
  fflush (stdout);
  return 0;
}
