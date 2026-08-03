/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  sim/scenario.c - deterministic simulation setup + step  *
*                                                          *
*  Shared by the numeric harness (sim/dump.c) and the       *
*  on-target benchmark (sim/bench.c), and compiled twice:   *
*  once against physics.c (16.16) and once, with            *
*  -DKOULES_FLOAT, against sim/physics_float.c (upstream).  *
*  Both builds must therefore see the same control flow and *
*  consume rand() in the same order, so the only difference *
*  between their outputs is the arithmetic.                 *
***********************************************************/

#include "koules.h"
#include "physics.h"
#include "scenario.h"

/* the pieces of global state the simulation reads but that
   normally live in koules.c / gameplan.c */
int             gameplan = COOPERATIVE;
int             gamemode = GAME;
int             sound = 0;
int             difficulty = 2;
unsigned char   rocketcolor[5] = {96, 160, 64, 96, 128};

/*
 * gameplan.c's create_letter()/allow_finder() decide whether a
 * destroyed ball leaves a letter behind.  Both consume rand(), so
 * the harness reproduces that rather than stubbing it to zero --
 * otherwise the RNG streams would drift apart the first time
 * anything died.
 */
static int      nletters = 0;

int
create_letter (void)
{
  if (rand () % 5)
    return 0;
  nletters++;
  return rand () % NLETTERS + 1;
}

int
allow_finder (void)
{
  return nletters < 3;
}

int
PlaySound (int s)
{
  (void) s;
  return 0;
}

/*
 * A fixed lattice rather than find_possition(), so the starting
 * state is identical in both builds without depending on the RNG
 * at all.  Objects are given crossing velocities so that the
 * O(n^2) collision loop is actually exercised: at n=30 the arena
 * is crowded and pairs overlap constantly, which is the worst
 * case the game can reach (gameplan.c caps nobjects at 30).
 */
static int      sim_n;

/*
 * Put object i back on its lattice square in its starting state.
 * Used by sim_setup() and, in the benchmark, by sim_respawn().
 */
void
sim_place (int i)
{
  int             type, gx, gy;
  int             cols = 6;

  if (i < nrockets)
    type = ROCKET;
  else if (i < nrockets + 2)
    type = BBALL;
  else if (i == sim_n - 1 && sim_n >= 10)
    type = HOLE;
  else
    type = BALL;

  object[i].type = type;
  object[i].ctype = type;
  object[i].live = (i < nrockets) ? 5 : 1;
  object[i].live1 = object[i].live;
  object[i].lineto = -1;
  object[i].thief = 0;
  object[i].time = 0;
  object[i].score = 0;
  object[i].letter = ' ';
  object[i].radius = radius (type);
  object[i].M = M (type);
  object[i].accel = ROCKET_SPEED;
  object[i].rotation = RAD (0);

  gx = 70 + (i % cols) * ((GAMEWIDTH - 140) / (cols - 1));
  gy = 60 + (i / cols) * ((GAMEHEIGHT - 120) / 5);
  object[i].x = OVI (gx);
  object[i].y = OVI (gy);

  /* crossing velocities, deterministic, roughly +/-2.5 units a
     frame -- fast enough to collide, slow enough not to tunnel
     through anything */
  object[i].fx = OVDIV (((i % 5) - 2) * 5, 2);
  object[i].fy = OVDIV (((i % 3) - 1) * 5, 2);
}

/*
 * Hold the population at n.  The benchmark reports "cost per frame
 * at n objects", so objects that die (into a hole, or off the edge)
 * have to come back or the later frames would be measuring a
 * smaller simulation than the label claims.  The revive pass is a
 * cheap O(n) scan and is inside the timed region, so if anything
 * the numbers are a shade pessimistic rather than optimistic.
 */
int
sim_respawn (void)
{
  int             i, live = 0;
  for (i = 0; i < nobjects; i++)
    {
      if (!object[i].live || object[i].type == CREATOR)
	sim_place (i);
      live++;
    }
  return live;
}

void
sim_setup (int n, unsigned int seed)
{
  int             i;

  srand (seed);
  nletters = 0;

  nrockets = 1;
  nobjects = n;
  sim_n = n;
  gameplan = COOPERATIVE;
  gamemode = GAME;
  dosprings = 1;
  randsprings = 30;
  npoint = 0;

  for (i = 0; i < MAXPOINT; i++)
    point[i].time = 0;

  for (i = 0; i < n; i++)
    sim_place (i);

  /* one spring so the lineto branch of update_forces() runs */
  if (n >= 4)
    object[3].lineto = 0;
}

/*
 * One frame, in the order game() runs them.  accel() is driven on
 * a fixed cadence so the sine/cosine and exhaust-particle paths
 * are timed too; upstream calls it from sprocess_keys() whenever
 * a player holds the thrust key, which in practice is most of the
 * time.
 */
void
sim_frame (int frame)
{
  int             r;

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
	if ((frame & 3) != 3)
	  accel (r, OVAL (1.0));
      }

  update_values ();
  update_forces ();
  colisions ();
  move_objects ();
  check_limit ();
}
