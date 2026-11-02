/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  Atari ST / STDL backend                                 *
*----------------------------------------------------------*
*  stdl/bench.c  the 256-colour screen's bench            *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

/*
 * Built into the benches (KOULDB.TOS, KOULDBC.TOS) only. With
 * KOULBNCH.CFG beside the program - one line, "colours frames seed
 * level flags [dump]": colours 8 (256 on a TT or a Falcon), 4
 * (sixteen) or t (sixteen on the display's own height, 240 rows on a
 * TT or a Falcon, the play still in the top 200), flags 4 a hash of
 * every page shown, written per frame to
 * KBHASH.BIN, with the page `dump` frames in written to KBPAGE.BIN -
 * it plays a level from a fixed seed with no input, every frame drawn,
 * and prints the time each part took, in STDL_GetHz38400's counts
 * turned into milliseconds a frame. A frame's time leaves out the
 * flip's wait for the VBL, so it is what the frame cost, not when the
 * display took it. The same seed gives the same game, so two builds'
 * hashes must agree.
 */

#include <interface.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../koules.h"
#include "../physics.h"
#include <stdl/stdl_bench.h>

/* STDL_GetHz38400's rate */
#define KB_HZ 38400ULL

extern int      kresponsive;
extern void     start ();
extern void     kbench_draw (int);
extern uint32_t ph_restore, ph_points, ph_obj, ph_over, ph_stat, ph_step,
                ph_flip;

int             kb_on;
static int      kb_frames = 300, kb_level = 25;
static unsigned long kb_seed = 1;
static unsigned kb_flags;
static int      kb_dump = -1;

void
kbench_config (void)
{
  FILE           *f = fopen ("KOULBNCH.CFG", "r");
  char            m = '4';

  if (f == NULL)
    return;
  if (fscanf (f, " %c %d %lu %d %u %d", &m, &kb_frames, &kb_seed,
	      &kb_level, &kb_flags, &kb_dump) >= 1)
    {
      kb_on = 1;
      kresponsive = m == '8' ? 1 : m == 't' ? 2 : 0;
    }
  fclose (f);
  if (kb_frames < 1)
    kb_frames = 1;
}

static int
cmpu (const void *a, const void *b)
{
  uint32_t        x = *(const uint32_t *) a, y = *(const uint32_t *) b;
  return x < y ? -1 : x > y;
}

/* hundredths of a millisecond, from clock units */
static unsigned long
cms (uint64_t units, unsigned long n)
{
  return (unsigned long) (units * 100000ULL / KB_HZ / (n ? n : 1));
}

#define MS(v) (v) / 100, (v) % 100

void
kbench_run (void)
{
  const STDL_PresentStats *st = STDL_GetPresentStats ();
  uint32_t       *ft = malloc ((size_t) kb_frames * sizeof (uint32_t));
  uint32_t       *fh = malloc ((size_t) kb_frames * sizeof (uint32_t));
  uint64_t        phys = 0, draw = 0, wait = 0;
  uint32_t        hash = 2166136261u, present0, groups0, flips0;
  unsigned long   p50, p95, mx;
  int             f;

  if (ft == NULL || fh == NULL)
    return;
  ksrand (kb_seed);
  nrockets = 1;
  lastlevel = maxlevel = kb_level;
  start ();
  tbreak = 0;
  present0 = st->present_time;
  groups0 = st->groups;
  flips0 = st->flips;
  ph_restore = ph_points = ph_obj = ph_over = ph_stat = ph_step = 0;
  ph_flip = 0;
  fprintf (stderr, "KB:start objects=%d\n", nobjects);
  for (f = 0; f < kb_frames; f++)
    {
      uint32_t        t0 = STDL_GetHz38400 (), t1, t2, w;
      uint32_t        flip0 = ph_flip, pres0 = st->present_time;

      update_values ();
      update_game ();
      update_forces ();
      colisions ();
      move_objects ();
      check_limit ();
      t1 = STDL_GetHz38400 ();
      kbench_draw (1);
      t2 = STDL_GetHz38400 ();
      /* the flip less its present is the wait for the VBL */
      w = (ph_flip - flip0) - (st->present_time - pres0);
      phys += t1 - t0;
      draw += t2 - t1 - w;
      wait += w;
      ft[f] = t2 - t1 - w;
      tbreak = 0;
      if (kb_flags & 4)
	{
	  /* the page shown, or on an ST the page now drawn into: the
	     same sequence either way, so two builds agree */
	  const uint32_t *p = st->shown != NULL
	    ? (const uint32_t *) st->shown : (const uint32_t *) backscreen->pixels;
	  const uint32_t  n = st->shown != NULL ? st->shown_bytes / 4
	    : (uint32_t) backscreen->stride * backscreen->h / 4;
	  uint32_t        i;

	  for (i = 0; i < n; i++)
	    hash = (hash ^ p[i]) * 16777619u;
	  fh[f] = hash;
	  /* the frames' hashes, and the page `kb_dump` frames in, to
	     find where two models part */
	  if (f == kb_dump)
	    {
	      FILE           *o = fopen ("KBPAGE.BIN", "wb");

	      if (o != NULL)
		{
		  fwrite (p, sizeof (uint32_t), n, o);
		  fclose (o);
		}
	    }
	}
    }
  if (kb_flags & 4)
    {
      FILE           *o = fopen ("KBHASH.BIN", "wb");

      if (o != NULL)
	{
	  fwrite (fh, sizeof (uint32_t), (size_t) kb_frames, o);
	  fclose (o);
	}
    }
  free (fh);
  qsort (ft, (size_t) kb_frames, sizeof (uint32_t), cmpu);
  p50 = cms (ft[kb_frames / 2], 1);
  p95 = cms (ft[kb_frames * 95 / 100], 1);
  mx = cms (ft[kb_frames - 1], 1);
  fprintf (stderr, "KB: depth=%d frames=%d seed=%lu level=%d flags=%u\n",
	   k256 ? 8 : 4, kb_frames, kb_seed, kb_level, kb_flags);
  fprintf (stderr, "KB: ms/frame phys=%lu.%02lu draw=%lu.%02lu "
	   "(present %lu.%02lu) wait=%lu.%02lu draw p50=%lu.%02lu "
	   "p95=%lu.%02lu max=%lu.%02lu\n",
	   MS (cms (phys, kb_frames)), MS (cms (draw, kb_frames)),
	   MS (cms (st->present_time - present0, kb_frames)),
	   MS (cms (wait, kb_frames)), MS (p50), MS (p95), MS (mx));
  fprintf (stderr, "KB: phases restore=%lu.%02lu step=%lu.%02lu "
	   "points=%lu.%02lu objects=%lu.%02lu over=%lu.%02lu "
	   "status=%lu.%02lu flip=%lu.%02lu\n",
	   MS (cms (ph_restore, kb_frames)), MS (cms (ph_step, kb_frames)),
	   MS (cms (ph_points, kb_frames)), MS (cms (ph_obj, kb_frames)),
	   MS (cms (ph_over, kb_frames)), MS (cms (ph_stat, kb_frames)),
	   MS (cms (ph_flip, kb_frames)));
  fprintf (stderr, "KB: groups/frame=%lu objects=%d hash=%08lx\n",
	   (unsigned long) ((st->groups - groups0)
			    / (st->flips - flips0 ? st->flips - flips0 : 1)),
	   nobjects, (unsigned long) hash);
  fprintf (stderr, "KB:done\n");
  free (ft);
}
