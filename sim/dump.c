/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  sim/dump.c - deterministic simulation oracle            *
*                                                          *
*  Runs N frames of pure simulation from a fixed seed and   *
*  writes per-frame object state as CSV.  Built twice --    *
*  once against upstream's float physics, once against the  *
*  16.16 physics -- and the two dumps are diffed by         *
*  sim/compare.py.  This is the correctness gate for the    *
*  conversion; the emulator is only ever used for timing.   *
*                                                          *
*  usage: dump [nobjects] [frames] [seed] [perturb]                   *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include <stdio.h>
#include <stdlib.h>
#include "koules.h"
#include "physics.h"
#include "scenario.h"

/* rotation is radians upstream and whole degrees in the fixed
   build; report degrees either way so the columns line up */
#ifdef KOULES_FLOAT
#include <math.h>
#define ROTDEG(r) ((double)(r) * 180.0 / M_PI)
#else
#define ROTDEG(r) ((double)(r))
#endif

int
main (int argc, char **argv)
{
  int             n = argc > 1 ? atoi (argv[1]) : 20;
  int             frames = argc > 2 ? atoi (argv[2]) : 300;
  unsigned int    seed = argc > 3 ? (unsigned int) atoi (argv[3]) : 12345;
  int             perturb = argc > 4 ? atoi (argv[4]) : 0;
  int             f, i;

  sim_setup (n, seed);

  /*
   * Control experiment.  Nudging one object by a single 16.16 ULP
   * (1/65536 of a game unit, about 1/130000 of a pixel) and
   * re-running the SAME build tells us how fast this simulation
   * decorrelates from any perturbation at all -- which is the
   * only fair yardstick for the float-vs-fixed divergence.
   */
  if (perturb)
    object[0].fx += OVDIV (1, 65536);

  printf ("frame,obj,type,live,score,x,y,fx,fy,M,rot\n");
  for (f = 0; f < frames; f++)
    {
      sim_frame (f);
      points1 ();
      for (i = 0; i < nobjects; i++)
	printf ("%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
		f, i, object[i].type, object[i].live, object[i].score,
		OV2D (object[i].x), OV2D (object[i].y),
		OV2D (object[i].fx), OV2D (object[i].fy),
		OV2D (object[i].M), ROTDEG (object[i].rotation));
    }
  return 0;
}
