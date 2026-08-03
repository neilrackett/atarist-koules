/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  C1995 JAHUSOFT                                          *
*        Jan Hubicka                                       *
*----------------------------------------------------------*
*   Copyright(c)1995,1996 by Jan Hubicka.See README for    *
*                    licence details.                      *
*----------------------------------------------------------*
*  physics.c - simulation, lifted out of koules.c and      *
*  converted from float to 16.16 fixed point for the ST    *
*                                                          *
*  Split out so the numeric harness (sim/dump.c) and the   *
*  on-target benchmark (sim/bench.c) can run the real code *
*  without linking a graphics backend.                     *
***********************************************************/

#include "koules.h"
#include "physics.h"

#ifdef SIM_COUNT
/* build-time instrumentation for the benchmark post-mortem; not
   compiled into anything that ships */
long            c_destroy, c_explode, c_parts, c_creator, c_cparts,
                c_rand, c_normalize, c_fixdiv, c_pairs, c_overlap;
#define CNT(x) (x)++
#else
#define CNT(x) do { } while (0)
#endif

int             nobjects = 8;
int             nrockets = 0;
int             npoint = 0;
Object          object[MAXOBJECT];
Point           point[MAXPOINT];

int             dosprings = 0;
int             randsprings = 0;

int             a_bballs, a_rockets, a_balls, a_holes, a_apples,
                a_inspectors, a_lunatics, a_eholes;

fix_t           ROCKET_SPEED = FIX (1.2);
fix_t           BALL_SPEED = FIX (1.2);
fix_t           BBALL_SPEED = FIX (1.2);
fix_t           SLOWDOWN = FIX (0.8);
fix_t           GUMM = FIX (20);

fix_t           BALLM = FIX (3);
fix_t           LBALLM = FIX (3);
fix_t           BBALLM = FIX (8);
fix_t           APPLEM = FIX (34);
fix_t           INSPECTORM = FIX (2);
fix_t           LUNATICM = FIX (3.14);
fix_t           ROCKETM = FIX (4);

static int      pcycle = 0;	/* recycle cursor once the list is full */

void
addpoint (CONST int x, CONST int y, CONST int xp, CONST int yp,
	  CONST int color, CONST int time)
{
  Point          *p;
  if (npoint < MAXPOINT)
    p = &point[npoint++];
  else
    {
      p = &point[pcycle];
      if (++pcycle >= MAXPOINT)
	pcycle = 0;
    }
  p->x = x / DIV;
  p->y = y / DIV;
  p->xp = xp / DIV;
  p->yp = yp / DIV;
  p->time = time;
  p->color = color;
}

/*
 * Particle step without drawing.  Already 24.8 fixed point
 * upstream, so there was no arithmetic to convert -- the change
 * here is that the list is compacted rather than scanned: a dead
 * particle is replaced by the last live one, so the loop only
 * touches particles that exist.  Order does not matter to
 * anything that reads point[].
 */
void
points1 (void)
{
  int             i = 0;
  while (i < npoint)
    {
      Point          *p = &point[i];
      if (--p->time <= 0)
	{
	  point[i] = point[--npoint];
	  continue;
	}
      p->x += p->xp;
      p->y += p->yp;
      i++;
    }
}

INLINE int
radius (CONST int type)
{
  switch (type)
    {
    case EHOLE:
    case HOLE:
      return (HOLE_RADIUS);
    case ROCKET:
      return (ROCKET_RADIUS);
    case BALL:
    case LBALL:
      return (BALL_RADIUS);
    case BBALL:
      return (BBALL_RADIUS);
    case APPLE:
      return (APPLE_RADIUS);
    case INSPECTOR:
      return (INSPECTOR_RADIUS);
    case LUNATIC:
      return (LUNATIC_RADIUS);
    }
  return (0);
}

static INLINE int
color (CONST int type, CONST int i, CONST int letter)
{
  switch (type)
    {
    case EHOLE:
      return (128);
    case HOLE:
      return (64);
    case ROCKET:
      return (rocketcolor[i]);
    case BALL:
      return (64);
    case LBALL:
      switch (letter)
	{
	case L_ACCEL:
	  return (128);
	case L_GUMM:
	  return (160);
	case L_THIEF:
	  return (192);
	case L_FINDER:
	  return (3 * 32);
	case L_TTOOL:
	  return (3 * 32);
	}

    case BBALL:
      return (128);
    case APPLE:
      return (64);
    case INSPECTOR:
      return (160);
    case LUNATIC:
      return (3 * 32);
    }
  return (0);
}

INLINE fix_t
M (CONST int type)
{
  switch (type)
    {
    case APPLE:
      return (APPLEM);
    case INSPECTOR:
      return (INSPECTORM);
    case LUNATIC:
      return (LUNATICM);
    case HOLE:
    case EHOLE:
      return (BBALLM);
    case ROCKET:
      return (ROCKETM);
    case BALL:
    case LBALL:
      return (BALLM);
    case BBALL:
      return (BBALLM);
    }
  return (0);
}

/*
 * The overlap test is done wholly in integer pixels.  In 16.16 a
 * squared distance of 640*640 would need 36 bits, and upstream
 * was comparing against integer radii anyway, so nothing is lost.
 */
int
find_possition (fix_t * x, fix_t * y, CONST int radius)
{
  int             x1, y1, i, y2 = 0;
  int             xp, yp, r;
rerand:;
  x1 = KRAND_N ((GAMEWIDTH - 60)) + 30;
  y1 = KRAND_N ((GAMEHEIGHT - 60)) + 30;
  for (i = 0; i < nobjects; i++)
    {
      xp = x1 - FIX2I (object[i].x);
      yp = y1 - FIX2I (object[i].y);
      r = radius + object[i].radius;
      if (xp * xp + yp * yp < r * r)
	{
	  y2++;
	  if (y2 > 10000)
	    return (0);
	  goto rerand;

	}
    }
  *x = FIXI (x1);
  *y = FIXI (y1);
  return (1);
}

/*
 * Upstream:
 *   object[i].x += object[i].fx * (GAMEWIDTH / 640.0 + 1) / 2;
 *
 * GAMEWIDTH was a runtime int, so that expression was two double
 * divisions plus a multiply *per object per axis per frame* and
 * the compiler could not hoist it.  With GAMEWIDTH fixed at 640
 * it is exactly 1.0, so the whole thing collapses to an add.
 */
void
move_objects (void)
{
  int             i;
  for (i = 0; i < nobjects; i++)
    if (object[i].type == CREATOR)
      {
	object[i].time--;
	if (object[i].time <= 0)
	  {
	    Effect (S_CREATOR2, next);
	    object[i].live = object[i].live1;
	    object[i].type = object[i].ctype;
	    if (object[i].type == ROCKET)
	      object[i].time = 200;
	    object[i].radius = radius (object[i].ctype);
	    object[i].M = M (object[i].ctype);
	  }
      }
    else if (object[i].live)
      {
#if MOVE_SCALE_IS_ONE
	object[i].x += object[i].fx;
	object[i].y += object[i].fy;
#else
	object[i].x += fixmul (object[i].fx, FIX (MOVE_SCALE));
	object[i].y += fixmul (object[i].fy, FIX (MOVE_SCALE));
#endif
      }
}

void
explosion (CONST int x, CONST int y, CONST int type, CONST int letter,
	   CONST int n)
{
  int             a, k, acc, parts;
  int             speed;
  int             color1;
  int             radius1 = radius (type);

  CNT (c_explode);
  parts = EXPLOSION_PARTS (radius1);
  if (parts <= 0)
    parts = 1;

  /* Spread "parts" fragments over the circle with an error
     accumulator rather than a per-fragment division. */
  a = 0;
  acc = 0;
  for (k = 0; k < parts; k++)
    {
      CNT (c_parts);
      CNT (c_rand);
      CNT (c_rand);
      CNT (c_rand);
      speed = KRAND_N (3096) + 10;
      color1 = color (type, n, letter) + (KRAND_N (32));
      addpoint (x * 256, y * 256,
		FIX2I (fixsin (a) * speed),
		FIX2I (fixcos (a) * speed),
		color1,
		KRAND_N (100) + 10);
      acc += 360;
      while (acc >= parts)
	{
	  acc -= parts;
	  a++;
	}
      if (a >= 360)
	a -= 360;
    }
}

static void
rocket_destroyed (CONST int player)
{
  int             i, nalive = 0, igagnant = 0;
  if (gamemode == GAME)
    switch (gameplan)
      {
      case DEATHMATCH:
	if (nrockets == 1)
	  return;
	for (i = 0; i < nrockets; i++)
	  if (object[i].type == ROCKET && object[i].live && i != player)
	    {
	      object[i].score += 100;
	      nalive++;
	      igagnant = i;
	    }
	if (nalive == 1)	/* winner bonus */
	  object[igagnant].score += 50;
      }
}

void
destroy (CONST int i)
{
  int             y;
  CNT (c_destroy);
  fix_t           r = FIXI (object[i].radius);
  if (object[i].x - r < 0)
    object[i].x = r + FIXONE, object[i].fx = -object[i].fx;
  if (object[i].y - r < 0)
    object[i].y = r + FIXONE, object[i].fy = -object[i].fy;
  if (object[i].x + r > FIXI (GAMEWIDTH))
    object[i].x = FIXI (GAMEWIDTH) - r - FIXONE, object[i].fx = -object[i].fx;
  if (object[i].y + r > FIXI (GAMEHEIGHT))
    object[i].y = FIXI (GAMEHEIGHT) - r - FIXONE, object[i].fy = -object[i].fy;
  switch (object[i].type)
    {
    case LBALL:
      Effect (S_DESTROY_BALL, next);
      object[i].live = 0, explosion (FIX2I (object[i].x), FIX2I (object[i].y),
				     object[i].type, object[i].letter, i);
      if (object[i].letter == L_THIEF && allow_finder ())
	{
	  object[i].live = 1;
	  object[i].letter = L_FINDER;
	}
      break;
    case APPLE:
      Effect (S_DESTROY_ROCKET, 0);
      object[i].live = 0, explosion (FIX2I (object[i].x), FIX2I (object[i].y),
				     object[i].type, object[i].letter, i);
      break;
    case BALL:
    case EHOLE:
    case BBALL:
    case INSPECTOR:
    case LUNATIC:
      Effect (S_DESTROY_BALL, next);
      if ((y = create_letter ()) != 0)
	{
	  object[i].type = LBALL;
	  object[i].M = LBALLM;
	  switch (y)
	    {
	    case 1:
	      object[i].letter = L_ACCEL;
	      break;
	    case 2:
	      object[i].letter = L_GUMM;
	      break;
	    case 3:
	      object[i].letter = L_THIEF;
	      break;
	    case 4:
	      object[i].letter = L_FINDER;
	      break;
	    case 5:
	      object[i].letter = L_TTOOL;
	      break;
	    }
	}
      else
	object[i].live = 0, explosion (FIX2I (object[i].x),
				       FIX2I (object[i].y), object[i].type,
				       object[i].letter, i);
      break;
    case ROCKET:
      Effect (S_DESTROY_ROCKET, 0);
      object[i].live1--, object[i].live--,
	explosion (FIX2I (object[i].x), FIX2I (object[i].y), object[i].type,
		   object[i].letter, i);
      rocket_destroyed (i);
      if (object[i].live)
	{
	  object[i].fx = 0;
	  object[i].fy = 0;
	  object[i].rotation = 0;
	  object[i].type = ROCKET;
	  object[i].accel = ROCKET_SPEED;
	  creator_rocket (i);
	}
      break;
    }
}

void
check_limit (void)
{
  int             i;
  for (i = 0; i < nobjects; i++)
    if (object[i].live)
      {
	fix_t           r = FIXI (object[i].radius);
	if (object[i].x - r < 0 || object[i].x + r >= FIXI (GAMEWIDTH) ||
	    object[i].y - r <= 0 || object[i].y + r >= FIXI (GAMEHEIGHT))
	  {
	    destroy (i);
	  }
      }
}

/*
 * count number of creatures
 */
void
update_values (void)
{
  int             i;
  a_holes = 0;
  a_rockets = 0;
  a_balls = 0;
  a_bballs = 0;
  a_apples = 0;
  a_eholes = 0;
  a_inspectors = 0;
  a_lunatics = 0;
  for (i = 0; i < nobjects; i++)
    {
      if (object[i].live)
	{
	  switch (object[i].type)
	    {
	    case HOLE:
	      a_holes++;
	      break;
	    case EHOLE:
	      a_eholes++;
	      break;
	    case ROCKET:
	      a_rockets++;
	      break;
	    case LBALL:
	    case BALL:
	      a_balls++;
	      break;
	    case BBALL:
	      a_bballs++;
	      break;
	    case APPLE:
	      a_apples++;
	      break;
	    case INSPECTOR:
	      a_inspectors++;
	      break;
	    case LUNATIC:
	      a_lunatics++;
	      break;
	    }
	}
      if (object[i].type == CREATOR)
	{
	  switch (object[i].ctype)
	    {
	    case BBALL:
	      a_bballs++;
	      break;
	    case HOLE:
	      a_holes++;
	      break;
	    case EHOLE:
	      a_eholes++;
	      break;
	    case ROCKET:
	      a_rockets++;
	      break;
	    case LBALL:
	    case BALL:
	      a_balls++;
	      break;
	    case APPLE:
	      a_apples++;
	      break;
	    case INSPECTOR:
	      a_inspectors++;
	      break;
	    case LUNATIC:
	      a_lunatics++;
	      break;
	    }
	}
    }

}

/*
 * accelerate rocket.  howmuch is 0..FIXONE, everything else is
 * cheating.  rotation is now integer degrees, so RAD() is the
 * identity and the sine table is indexed directly.
 */
void
accel (CONST int i, CONST fix_t howmuch)
{
  int             y;
  fix_t           k = fixmul (howmuch, object[i].accel);
  int             rot = object[i].rotation;

  object[i].fx += fixmul (k, fixsin (rot));
  object[i].fy += fixmul (k, fixcos (rot));

  for (y = 0; y < 5 / DIV / DIV; y++)
    {
      int             p;
      p = KRAND_N (45) - 22;
      addpoint (FIX2I (object[i].x) * 256,
		FIX2I (object[i].y) * 256,
		FIX2I (fixmul (object[i].fx -
			       fixmul (k * 10, fixsin (rot + p)),
			       FIXI (KRAND_N (512)))),
		FIX2I (fixmul (object[i].fy -
			       fixmul (k * 10, fixcos (rot + p)),
			       FIXI (KRAND_N (512)))),
		rocket (KRAND_N (16)), 10);
    }
}

#define MIN(a,b) ((a)>(b)?(b):(a))
/*
 * Make creations happen as coalescing circular cloud.  Do this by
 * creating random points within circle defined from center of screen, and
 * giving them velocity towards desired final point.
 *
 * Upstream took a sqrt here and then divided by it again:
 *   r = sqrt(dx*dx+dy*dy);  r = (r*radius/r1)/r*0.9;
 * the length cancels, so the scale is the loop invariant
 * radius*0.9/r1.  Hoisted into k16 (16.16) below.
 */
void
creators_points (int radius, int x1, int y1, int color1)
{
  int             z, x, y, x2, y2;
  int             time = 50;
  int             midX, midY, r2, r1;
  long            k16;

  midX = GAMEWIDTH / 2;
  midY = GAMEHEIGHT / 2;
  r2 = r1 = MIN (midX, midY);
  r2 *= r2;

  k16 = ((long) radius * 59005L) / r1;	/* 0.9 * 65536 = 58982.4 */

  CNT (c_creator);
  z = CREATOR_PARTS (radius);
  while (z--)
    {
      CNT (c_cparts);
      do
	{
	  CNT (c_rand);
	  CNT (c_rand);
	  x = KRAND_N (GAMEWIDTH);
	  y = KRAND_N (GAMEHEIGHT);
	}
      while (((x - midX) * (x - midX) + (y - midY) * (y - midY)) > r2);
      x2 = x1 + (int) (((long) (x - midX) * k16) >> 16);
      y2 = y1 + (int) (((long) (y - midY) * k16) >> 16);

      addpoint (x * 256, y * 256,
		(x2 - x) * 256 / (time),
		(y2 - y) * 256 / (time),
		color1 + (KRAND_N (32)),
		time);
    }
}

void
creator (CONST int type)
{
  int             i;
  int             color1 = color (type, 0, 0);
  for (i = nrockets; i < nobjects && (object[i].live ||
				      object[i].type == CREATOR);
       i++);
  if (i >= MAXOBJECT)
    return;
  if (!find_possition (&object[i].x, &object[i].y, radius (type)))
    return;
  if (i >= nobjects)
    nobjects = i + 1;
  object[i].live = 0;
  object[i].live1 = 1;
  object[i].lineto = -1;
  object[i].ctype = type;
  object[i].fx = 0;
  object[i].fy = 0;
  object[i].time = 50;
  object[i].rotation = 0;
  object[i].type = CREATOR;
  object[i].M = M (type);
  object[i].radius = radius (type);
  object[i].accel = ROCKET_SPEED;
  object[i].letter = ' ';
  creators_points (object[i].radius, FIX2I (object[i].x),
		   FIX2I (object[i].y), color1);
  Effect (S_CREATOR1, 0);
}

void
creator_rocket (CONST int i)
{
  int             type = ROCKET;
  int             color1 = color (ROCKET, i, 0);
  if (!find_possition (&object[i].x, &object[i].y, radius (type)))
    return;
  if (sound)
    object[i].live1 = object[i].live;
  object[i].live = 0;
  object[i].thief = 0;
  object[i].ctype = type;
  object[i].lineto = -1;
  object[i].fx = 0;
  object[i].fy = 0;
  object[i].time = 50;
  object[i].rotation = 0;
  object[i].type = CREATOR;
  object[i].M = ROCKETM;
  object[i].radius = ROCKET_RADIUS;
  object[i].accel = ROCKET_SPEED;
  object[i].letter = ' ';
  creators_points (ROCKET_RADIUS, FIX2I (object[i].x), FIX2I (object[i].y),
		   color1);
}

void
update_forces (void)
{
  int             i;
  int             r;
  fix_t           xp, yp;
  int             xi, yi;

  for (i = 0; i < nobjects; i++)
    {
      if (object[i].live)
	{
	  if (object[i].lineto != -1)
	    {
	      if (!object[object[i].lineto].live)
		object[i].lineto = -1;
	      else if (object[i].lineto == i)
		object[i].lineto = -1;
	      else
		{
		  int             force;
		  int             lt = object[i].lineto;
		  xp = object[i].x - object[lt].x;
		  yp = object[i].y - object[lt].y;
		  /* upstream truncated the sqrt to int here */
		  force = IDIST (FIX2I (xp), FIX2I (yp));
		  if (force >= 2 * SPRINGSIZE || gameplan == COOPERATIVE)
		    {
		      fix_t           s;
		      force = force - SPRINGSIZE;
		      if (force < 0)
			force *= 3;
		      force = force / SPRINGSTRENGTH;
		      s = fixmul (FIXI (force), BALL_SPEED);
		      {
			fix_t           nx = xp, ny = yp;
			fix_normalize (&nx, &ny, fixdiv (s, object[i].M));
			object[i].fx -= nx;
			object[i].fy -= ny;
		      }
		      {
			fix_t           nx = xp, ny = yp;
			fix_normalize (&nx, &ny, fixdiv (s, object[lt].M));
			object[lt].fx += nx;
			object[lt].fy += ny;
		      }
		    }
		}
	    }
	  if (object[i].type == ROCKET && object[i].time)
	    object[i].time--;
	  if (object[i].type == ROCKET && !object[i].time)
	    {
	      for (r = 0; r < nobjects; r++)
		{
		  if (object[r].live && !object[r].time
		      && object[r].type == EHOLE)
		    {
		      int             distance;
		      fix_t           gravity, cap;
		      xp = object[r].x - object[i].x;
		      yp = object[r].y - object[i].y;
		      distance = IDIST (FIX2I (xp), FIX2I (yp));
		      if (distance == 0)
			distance = 1;
		      gravity = fixdiv (BALL_SPEED *
					(gameplan == COOPERATIVE ? 200 : 50),
					FIXI (distance));
		      cap = (BALL_SPEED * 4) / 5;
		      if (gravity > cap)
			gravity = cap;
		      fix_normalize (&xp, &yp, gravity);
		      object[i].fx += xp;
		      object[i].fy += yp;
		    }
		}

	    }
	  if (object[i].type == BALL || object[i].type == LBALL
	      || object[i].type == BBALL || object[i].type == LUNATIC)
	    {
	      int             frocket = -1;
	      long            d = 640L * 640L;
	      xi = FIX2I (object[i].x);
	      yi = FIX2I (object[i].y);
	      for (r = 0; r < nrockets; r++)
		{
		  if (object[r].live && !object[r].time)
		    {
		      int             dx = FIX2I (object[r].x) - xi;
		      int             dy = FIX2I (object[r].y) - yi;
		      long            q = (long) dx *dx + (long) dy *dy;
		      if (q < d)
			d = q, frocket = r;
		    }
		}
	      if (frocket != -1)
		xp = object[frocket].x - object[i].x,
		  yp = object[frocket].y - object[i].y;
	      else
		xp = FIXI (GAMEWIDTH / 2) - object[i].x,
		  yp = FIXI (GAMEHEIGHT / 2) - object[i].y;
	      if (object[i].type == LUNATIC && !rand () % 4)
		{
		  xp = FIXI (rand () & 1023);
		  yp = FIXI ((rand () & 1023) + 1);
		}
	      switch (object[i].type)
		{
		case BBALL:
		  fix_normalize (&xp, &yp, BBALL_SPEED);
		  break;
		case BALL:
		case LUNATIC:
		case LBALL:
		  fix_normalize (&xp, &yp, BALL_SPEED);
		  break;
		}
	      object[i].fx += xp;
	      object[i].fy += yp;
	    }
	  object[i].fx = fixmulf (object[i].fx, SLOWDOWN),
	    object[i].fy = fixmulf (object[i].fy, SLOWDOWN);
	}
    }
}

/*
 * O(n^2/2) over every live pair.  This is the frame's hot loop,
 * so the overlap test is kept in plain integer pixels: two long
 * subtracts, two 16x16 multiplies and a compare, with object i's
 * coordinates and radius hoisted out of the inner loop.  In 16.16
 * dx*dx would overflow anyway.
 *
 * Object is 44 bytes, so object[y] costs gcc 4.6 a __mulsi3 call
 * per access on a 68000 -- which is why the pair test walks a
 * pointer instead of indexing.
 *
 * Measured dead end, do not retry without re-measuring: copying
 * the coordinates into parallel int arrays first made it *slower*
 * (68.6ms/frame against 64.7 at n=20) because the live flag still
 * had to be read through the struct, so the pass was paid for and
 * nothing was saved.  Same checksums, purely a pessimisation.
 * atarist-stdl's AGENTS.md warns about exactly this.
 */
void
colisions (void)
{
  int             i, y;
  int             colize = 0;
  static int      ctime = 0;
  fix_t           xp, yp, gummfactor;

  for (i = 0; i < nobjects; i++)
    if (object[i].live)
      {
	Object         *CONST oi = &object[i];
	CONST int       xi = FIX2I (oi->x);
	CONST int       yi = FIX2I (oi->y);
	CONST int       ri = oi->radius;
	CONST int       n = nobjects;
	Object         *oy = &object[i + 1];

	for (y = i + 1; y < n; y++, oy++)
	  if (oy->live)
	    {
	      int             dx = FIX2I (oy->x) - xi;
	      int             dy = FIX2I (oy->y) - yi;
	      int             rs = oy->radius + ri;

	      CNT (c_pairs);
	      if (dx * dx + dy * dy < rs * rs)
		{
		  CNT (c_overlap);
		  xp = oy->x - oi->x;
		  yp = oy->y - oi->y;
		  colize = 1;
		  if (oi->type == HOLE || oi->type == EHOLE)
		    {
		      if (oy->type != APPLE)
			destroy (y);
		      if (oi->type == EHOLE)
			destroy (i);
		      continue;
		    }
		  if (oy->type == HOLE || oy->type == EHOLE)
		    {
		      if (oi->type != APPLE)
			destroy (i);
		      if (oy->type == EHOLE)
			destroy (y);
		      continue;
		    }
		  if (oi->type == ROCKET)
		    {
		      if (oy->thief == 1 && oi->thief == 1)
			{
			  fix_t           tmp;
			  tmp = oi->M;
			  oi->M = oy->M;
			  oy->M = tmp;
			  oi->thief = 0;
			  oy->thief = 0;
			}
		      if (oy->type == BBALL && oi->thief == 1)
			{
			  oi->M += oy->M - M (BALL);
			  oi->thief = 0;
			  oy->M = M (BALL);
			}
		      else if (oy->type == ROCKET
			       && oi->thief == 1)
			{
			  oi->M += oy->M - M (ROCKET);
			  oi->accel += oy->accel - ROCKET_SPEED;
			  oi->thief = 0;
			  oy->M = M (oi->type);
			  oy->accel = ROCKET_SPEED - FIX (A_ADD);
			}
		      if (oi->type == ROCKET && oy->thief == 1)
			{
			  oy->M += oi->M - M (ROCKET);
			  oy->accel += oi->accel - ROCKET_SPEED;
			  oy->thief = 0;
			  oi->M = M (oy->type);
			  oi->accel = ROCKET_SPEED - FIX (A_ADD);
			}
		      if (gameplan == COOPERATIVE)
			oi->score++;
		      if (oy->letter == L_ACCEL)
			oi->accel += FIX (A_ADD),
			  oi->score += 10;
		      if (oy->letter == L_GUMM)
			oi->M += FIX (M_ADD),
			  oi->score += 10;
		      if (oy->letter == L_THIEF)
			oi->M = M (oi->type),
			  oi->accel = ROCKET_SPEED - FIX (A_ADD),
			  oi->score -= 30;
		      if (oy->letter == L_FINDER)
			{
			  oi->accel += FIX (A_ADD) * (KRAND_N (5));
			  oi->M += FIX (M_ADD) * (KRAND_N (10));
			  oi->score += 30;
			}
		      if (oy->letter == L_TTOOL)
			{
			  oi->thief = 1;
			  oi->score += 30;
			}

		      oy->letter = ' ';
		      if (oy->type == LBALL)
			oy->type = BALL;
		      if (oy->type == BALL && dosprings
			  && !(KRAND_N (randsprings)))
			oy->lineto = i;

		      if (gameplan == DEATHMATCH && oy->type == ROCKET
			  && dosprings && !(KRAND_N ((2 * randsprings))))
			oy->lineto = i;
		    }
		  if (oy->type == LUNATIC)
		    {
		      gummfactor = -fixdiv (ROCKETM, LUNATICM);
		    }
		  else if (oi->type == LUNATIC)
		    {
		      gummfactor = -fixdiv (LUNATICM, ROCKETM);
		    }
		  else
		    gummfactor = fixdiv (oi->M, oy->M);
		  {
		    fix_t           nx = xp, ny = yp;
		    fix_normalize (&nx, &ny, fixmul (gummfactor, GUMM));
		    oy->fx += nx;
		    oy->fy += ny;
		  }
		  {
		    fix_t           nx = xp, ny = yp;
		    fix_normalize (&nx, &ny,
				   fixdiv (GUMM, gummfactor));
		    oi->fx -= nx;
		    oi->fy -= ny;
		  }
		  if (oi->type == ROCKET && oi->time)
		    oi->fx = 0,
		      oi->fy = 0;
		  if (oy->type == ROCKET && oy->time)
		    oy->fx = 0,
		      oy->fy = 0;
		  if (oy->type == INSPECTOR
		      && oi->type == ROCKET)
		    {
		      oy->fx = 0,
			oy->fy = 0;
		      oi->fx *= -2,
			oi->fy *= -2;
		    }
		}
	    }
      }
  if (colize && !ctime)
    {
      Effect (S_COLIZE, next);
      ctime = 4;
    }
  if (ctime)
    ctime--;
}
