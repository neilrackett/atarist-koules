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
 * Built into KOULDB.TOS only. With KOULBNCH.CFG beside the program -
 * one line, "colours frames seed level flags [dump]": colours 8 (256
 * on a TT or a Falcon) or 4 (sixteen), flags 1 no VBL wait, 4 a hash
 * of every page shown, written per frame to KBHASH.BIN, with the page
 * `dump` frames in written to KBPAGE.BIN - it plays a level from a
 * fixed seed with no input, every frame drawn, and prints the time
 * each part took, in STDL_RespClock's units (38,400 a second) turned
 * into milliseconds a frame. The same seed gives the same game, so two
 * builds' hashes must agree.
 */

#include <interface.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../koules.h"

extern int      kresponsive;
extern void     start ();
extern void     update_values (void);
extern void     update_game ();
extern void     update_forces (void);
extern void     colisions (void);
extern void     move_objects (void);
extern void     check_limit (void);
extern void     kbench_draw (int);
extern uint32_t ph_restore, ph_points, ph_obj, ph_over, ph_stat, ph_step,
                ph_flip;

int             kb_on;
static int      kb_frames = 300, kb_level = 25;
static unsigned long kb_seed = 1;
static unsigned kb_flags = 1;
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
      kresponsive = m == '8';
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
  return (unsigned long) (units * 100000ULL / 38400ULL / (n ? n : 1));
}

#define MS(v) (v) / 100, (v) % 100

void
kbench_run (void)
{
  STDL_RespStats *st = STDL_RespGetStats ();
  uint32_t       *ft = malloc ((size_t) kb_frames * sizeof (uint32_t));
  uint32_t       *fh = malloc ((size_t) kb_frames * sizeof (uint32_t));
  uint64_t        phys = 0, draw = 0;
  uint32_t        hash = 2166136261u, present0, cells0, flips0;
  unsigned long   p50, p95, mx;
  int             f;

  if (ft == NULL || fh == NULL)
    return;
  ksrand (kb_seed);
  nrockets = 1;
  lastlevel = maxlevel = kb_level;
  start ();
  tbreak = 0;
  STDL_RespBench (kb_flags & STDL_RESPB_NOWAIT);
  present0 = st->present;
  cells0 = st->cells;
  flips0 = st->flips;
  ph_restore = ph_points = ph_obj = ph_over = ph_stat = ph_step = 0;
  ph_flip = 0;
  fprintf (stderr, "KB:start objects=%d\n", nobjects);
  for (f = 0; f < kb_frames; f++)
    {
      uint32_t        t0 = STDL_RespClock (), t1, t2;

      update_values ();
      update_game ();
      update_forces ();
      colisions ();
      move_objects ();
      check_limit ();
      t1 = STDL_RespClock ();
      kbench_draw (1);
      t2 = STDL_RespClock ();
      phys += t1 - t0;
      draw += t2 - t1;
      ft[f] = t2 - t1;
      tbreak = 0;
      if (kb_flags & 4)
	{
	  /* the page shown, or on an ST the page now drawn into: the
	     same sequence either way, so two builds agree */
	  const uint32_t *p = st->page != NULL
	    ? (const uint32_t *) st->page : (const uint32_t *) backscreen->pixels;
	  const uint32_t  n = st->page != NULL ? st->page_bytes / 4
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
		  fwrite (st->page, 1, st->page_bytes, o);
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
	   "(present %lu.%02lu) draw p50=%lu.%02lu p95=%lu.%02lu "
	   "max=%lu.%02lu\n",
	   MS (cms (phys, kb_frames)), MS (cms (draw, kb_frames)),
	   MS (cms (st->present - present0, kb_frames)), MS (p50), MS (p95),
	   MS (mx));
  fprintf (stderr, "KB: phases restore=%lu.%02lu step=%lu.%02lu "
	   "points=%lu.%02lu objects=%lu.%02lu over=%lu.%02lu "
	   "status=%lu.%02lu flip=%lu.%02lu\n",
	   MS (cms (ph_restore, kb_frames)), MS (cms (ph_step, kb_frames)),
	   MS (cms (ph_points, kb_frames)), MS (cms (ph_obj, kb_frames)),
	   MS (cms (ph_over, kb_frames)), MS (cms (ph_stat, kb_frames)),
	   MS (cms (ph_flip, kb_frames)));
  fprintf (stderr, "KB: cells/frame=%lu objects=%d hash=%08lx\n",
	   (unsigned long) ((st->cells - cells0)
			    / (st->flips - flips0 ? st->flips - flips0 : 1)),
	   nobjects, (unsigned long) hash);
  fprintf (stderr, "KB:done\n");
  free (ft);
}
