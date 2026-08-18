/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  sim/bench.c - headless physics benchmark                *
*                                                          *
*  No rendering, no backend, no input: just the simulation *
*  run for a fixed number of frames with the 200Hz system  *
*  tick read either side.  Built twice (float / 16.16) and *
*  run under Hatari on --machine st, which is the 8MHz     *
*  correctness floor.  The frame budget is 40ms total      *
*  (game() sets VfTime = 1000000/25).                      *
*                                                          *
*  The 200Hz counter ticks every 5ms, so each measurement  *
*  runs enough frames to make quantisation irrelevant.     *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include <stdio.h>
#include "koules.h"
#include "physics.h"
#include "scenario.h"

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

static CONST int counts[] = {5, 10, 20, 30};
#define NCOUNTS (int)(sizeof counts / sizeof counts[0])

/* enough frames that the 5ms tick granularity is under 1% */
#define FRAMES 400

static long
checksum (void)
{
  long            s = 0;
  int             i;
  for (i = 0; i < nobjects; i++)
    s += (long) object[i].x + (long) object[i].y + object[i].score
      + object[i].live * 7 + object[i].type * 31;
  return s;
}

int
main (void)
{
  int             c, f, live = 0;
  unsigned long   t0, t1;
  long            phys[NCOUNTS], part[NCOUNTS], sums[NCOUNTS];

#ifdef __m68k__
  /* $4BA is supervisor-only; stay there for the run (see
     atarist-stdl AGENTS.md -- leaving at a different stack depth
     is what crashes, so we simply do not leave) */
  Super (0L);
#endif

  for (c = 0; c < NCOUNTS; c++)
    {
      /* physics only */
      sim_setup (counts[c], 12345);
      t0 = TICKS ();
      for (f = 0; f < FRAMES; f++)
	{
	  sim_frame (f);
	  live = sim_respawn ();
	}
      t1 = TICKS ();
      phys[c] = (long) (t1 - t0) * 5L;
      sums[c] = checksum ();

      /* particle sweep on its own -- MAXPOINT slots scanned
         unconditionally, independent of object count */
      sim_setup (counts[c], 12345);
      t0 = TICKS ();
      for (f = 0; f < FRAMES; f++)
	points1 ();
      t1 = TICKS ();
      part[c] = (long) (t1 - t0) * 5L;
    }

  printf ("BENCH-START %s\n", BUILD);
  printf ("frames %d, MAXPOINT %d\n", FRAMES, MAXPOINT);
  printf ("n  physics_ms/frame  particles_ms/frame  total  checksum\n");
  for (c = 0; c < NCOUNTS; c++)
    printf ("%2d %5ld.%02ld %11ld.%02ld %11ld.%02ld  %ld\n",
	    counts[c],
	    phys[c] / FRAMES, (phys[c] % FRAMES) * 100 / FRAMES,
	    part[c] / FRAMES, (part[c] % FRAMES) * 100 / FRAMES,
	    (phys[c] + part[c]) / FRAMES,
	    ((phys[c] + part[c]) % FRAMES) * 100 / FRAMES,
	    sums[c]);
  printf ("live objects held at n throughout: %d\n", live);
  printf ("BENCH-DONE %s\n", BUILD);
  fflush (stdout);
  return 0;
}
