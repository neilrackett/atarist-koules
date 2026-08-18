/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  C1995 JAHUSOFT / Jan Hubicka -- see README for licence  *
*----------------------------------------------------------*
*  sim/physics_float.c                                     *
*                                                          *
*  Upstream's simulation, lifted out of koules.c UNCHANGED *
*  apart from the globals moved in below and the six       *
*  functions that lost their "static".  This is the        *
*  reference implementation: sim/dump.c runs it and the    *
*  fixed point physics.c side by side and diffs the state, *
*  and sim/bench.c times it on target so the speedup is    *
*  measured rather than assumed.                           *
*                                                          *
*  Do not "improve" anything in here.  Its whole value is  *
*  being the original.                                     *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include "koules.h"
#include "physics.h"

int             nobjects = 8;
int             nrockets = 0;
int             npoint = 0;
Object          object[MAXOBJECT];
Point           point[MAXPOINT];

int             dosprings = 0;
int             randsprings = 0;

int             a_bballs, a_rockets, a_balls, a_holes, a_apples,
                a_inspectors, a_lunatics, a_eholes;

float           ROCKET_SPEED = 1.2;
float           BALL_SPEED = 1.2;
float           BBALL_SPEED = 1.2;
float           SLOWDOWN = 0.8;
float           GUMM = 20;

float           BALLM = 3;
float           LBALLM = 3;
float           BBALLM = 8;
float           APPLEM = 34;
float           INSPECTORM = 2;
float           LUNATICM = 3.14;
float           ROCKETM = 4;

static int      pcycle = 0;

void
addpoint (CONST int x, CONST int y, CONST int xp, CONST int yp, CONST int color, CONST int time)
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

void
points1 ()
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
INLINE float
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

int
find_possition (float *x, float *y, CONST int radius)	/* was CONST float; callers only ever pass radius(type) */
{
  int             x1, y1, i, y2 = 0;
  float           xp, yp;
rerand:;
  x1 = KRAND_N ((GAMEWIDTH - 60)) + 30;
  y1 = KRAND_N ((GAMEHEIGHT - 60)) + 30;
  for (i = 0; i < nobjects; i++)
    {
      xp = x1 - object[i].x;
      yp = y1 - object[i].y;
      if (xp * xp + yp * yp < (radius + object[i].radius) *
	  (radius + object[i].radius))
	{
	  y2++;
	  if (y2 > 10000)
	    return (0);
	  goto rerand;

	}
    }
  *x = (float) x1;
  *y = (float) y1;
  return (1);
}

INLINE void
normalize (float *x, float *y, float size)
{
  float           length = sqrt ((*x) * (*x) + (*y) * (*y));
  if (length == 0)
    length = 1;
  *x *= size / length;
  *y *= size / length;
}


void
move_objects ()
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
	object[i].x += object[i].fx * (GAMEWIDTH / 640.0 + 1) / 2;
	object[i].y += object[i].fy * (GAMEWIDTH / 640.0 + 1) / 2;
      }
}




void
explosion (CONST int x, CONST int y, CONST int type, CONST int letter, CONST int n)
{
  float           i;
  int             speed;
  int             color1;
  int             radius1 = radius (type);
#ifdef NETSUPPORT
  if (server)
    {
      Explosion (x, y, type, letter, n);
      return;
    }
#endif
  for (i = 0; i < RAD (360); i += RAD (360.0) * DIV * DIV / radius1 / radius1 / M_PI)
    {
      speed = KRAND_N (3096) + 10;
      if (DIV == 1)
	color1 = color (type, n, letter) + (KRAND_N (16));
      else
	color1 = color (type, n, letter) + (KRAND_N (32));
      addpoint (x * 256, y * 256,
		sin (i) * (speed),
		cos (i) * (speed),
		color1,
		KRAND_N (100) + 10);
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
  if (object[i].x - object[i].radius < 0)
    object[i].x = object[i].radius + 1, object[i].fx *= -1;
  if (object[i].y - object[i].radius < 0)
    object[i].y = object[i].radius + 1, object[i].fy *= -1;
  if (object[i].x + object[i].radius > GAMEWIDTH)
    object[i].x = GAMEWIDTH - object[i].radius - 1, object[i].fx *= -1;
  if (object[i].y + object[i].radius > GAMEHEIGHT)
    object[i].y = GAMEHEIGHT - object[i].radius - 1, object[i].fy *= -1;
  switch (object[i].type)
    {
    case LBALL:
      Effect (S_DESTROY_BALL, next);
      object[i].live = 0, explosion (object[i].x, object[i].y, object[i].type, object[i].letter, i);
      if (object[i].letter == L_THIEF && allow_finder ())
	{
	  object[i].live = 1;
	  object[i].letter = L_FINDER;
	}			/* else
				   if (object[i].letter == L_FINDER )
				   {
				   object[i].live = 1;
				   object[i].letter = L_THIEF;
				   } */
      break;
    case APPLE:
      Effect (S_DESTROY_ROCKET, 0);
      object[i].live = 0, explosion (object[i].x, object[i].y, object[i].type, object[i].letter, i);
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
	      /*case 3:
	         object[i].letter = L_MUGG;
	         break;
	         case 4:
	         object[i].letter = L_SLOW;
	         break;
	         case 5:
	         object[i].letter = L_WIZZ;
	         break;
	         case 6:
	         object[i].letter = L_FUCK;
	         break; */
	    }
	}
      else
	object[i].live = 0, explosion (object[i].x, object[i].y, object[i].type, object[i].letter, i);
      break;
    case ROCKET:
      Effect (S_DESTROY_ROCKET, 0);
      object[i].live1--, object[i].live--, explosion (object[i].x, object[i].y, object[i].type, object[i].letter, i);
      rocket_destroyed (i);
      if (object[i].live)
	{
	  /*object[i].x = KRAND_N ((GAMEWIDTH-60))+30;
	     object[i].y = KRAND_N ((GAMEHEIGHT-60))+30; */
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
check_limit ()
{
  int             i;
  for (i = 0; i < nobjects; i++)
    if (object[i].live)
      {
	if (object[i].x - object[i].radius < 0 || object[i].x + object[i].radius >= GAMEWIDTH ||
	    object[i].y - object[i].radius <= 0 || object[i].y + object[i].radius >= GAMEHEIGHT)
	  {
	    destroy (i);
	  }
      }
}



/*
 * count number of creatures
 */
void
update_values ()
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
 * accelerate rocket
 */
void
/* howmuch is between 0 and 1, everything else is cheating */
accel (CONST int i, CONST oval_t howmuch)	/* was CONST double; only ever called with 1.0 */
{
  int             y;
#ifdef NETSUPPORT
  if (server)
    acceled[i] = 1;
#endif
#ifdef NETSUPPORT
  if (!client)
#endif
    {
      object[i].time = 0;
       object[i].fx += howmuch * sin (object[i].rotation) * object[i].accel,
       object[i].fy += howmuch * cos (object[i].rotation) * object[i].accel;
#ifdef NETSUPPORT
      if (!server)
#endif
	for (y = 0; y < 5 / DIV / DIV; y++)
	  {
	    float           p;
	    p = RAD (KRAND_N (45) - 22);
	    addpoint (object[i].x * 256,
		      object[i].y * 256,
		      (object[i].fx - howmuch * sin (object[i].rotation + p) * object[i].accel * 10) * (KRAND_N (512)),
		      (object[i].fy - howmuch * cos (object[i].rotation + p) * object[i].accel * 10) * (KRAND_N (512)),
		      rocket (KRAND_N (16)), 10);
	  }
    }
#ifdef NETSUPPORT
  else
    {
      for (y = 0; y < 5 / DIV / DIV; y++)
	{
	  float           p;
	  p = RAD (KRAND_N (30) - 15);
	  addpoint (object[i].x * 256,
		    object[i].y * 256,
		    (-sin (object[i].rotation + p) * ROCKET_SPEED * 5) * (KRAND_N (512)),
		    (-cos (object[i].rotation + p) * ROCKET_SPEED * 5) * (KRAND_N (512)),
		    rocket (KRAND_N (16)), 10);
	}
    }
#endif
}

#define MIN(a,b) ((a)>(b)?(b):(a))
/*
 * Make creations happen as coalescing circular cloud.  Do this by
 * creating random points within circle defined from center of screen, and
 * giving them velocity towards desired final point.
 */

void
creators_points (int radius, int x1, int y1, int color1)
{
    int             z, x, y, x2, y2;
    double r;
    int             time = 50;
    int             midX, midY, r2,r1;

    midX = GAMEWIDTH / 2;
    midY = GAMEHEIGHT / 2;
    r2 = r1 = MIN(midX, midY);
    r2 *= r2;

    z = radius * radius * M_PI / DIV / DIV;
    while (z--) {
	do {
	    x = rand() % GAMEWIDTH;
	    y = rand() % GAMEHEIGHT;
	} while (((x-midX)*(x-midX) + (y-midY)*(y-midY)) > r2);
	r=sqrt((double)((x-midX)*(x-midX) + (y-midY)*(y-midY)));
	r=(r*radius/r1)/r*0.9;
	x2=x1+(x-midX)*r;
	y2=y1+(y-midY)*r;

	addpoint(x * 256, y * 256,
		    (x2 - x) * 256 / (time),
		    (y2 - y) * 256 / (time),
		    color1 + (rand() % (DIV == 1 ? 16 : 32)),
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
  object[i].fx = 0.0;
  object[i].fy = 0.0;
  object[i].time = 50;
  object[i].rotation = 0;
  object[i].type = CREATOR;
  object[i].M = M (type);
  object[i].radius = radius (type);
  object[i].accel = ROCKET_SPEED;
  object[i].letter = ' ';
#ifdef NETSUPPORT
  if (server)
    CreatorsPoints (object[i].radius, object[i].x, object[i].y, color1);
  else
#endif
    creators_points (object[i].radius, object[i].x, object[i].y, color1);
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
  object[i].fx = 0.0;
  object[i].fy = 0.0;
  object[i].time = 50;
  object[i].rotation = 0;
  object[i].type = CREATOR;
  object[i].M = ROCKETM;
  object[i].radius = ROCKET_RADIUS;
  object[i].accel = ROCKET_SPEED;
  object[i].letter = ' ';
#ifdef NETSUPPORT
  if (server)
    CreatorsPoints (ROCKET_RADIUS, object[i].x, object[i].y, color1);
  else
#endif
    creators_points (ROCKET_RADIUS, object[i].x, object[i].y, color1);
}




void
update_forces ()
{
  int             i;
  int             r;
  float           d;
  float           xp, yp;
  int             frocket = 0;
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
		  xp = object[i].x - object[object[i].lineto].x;
		  yp = object[i].y - object[object[i].lineto].y;
		  force = sqrt (xp * xp + yp * yp);
		  if (force >= 2 * SPRINGSIZE || gameplan == COOPERATIVE)
		    {
		      force = force - SPRINGSIZE;
		      if (force < 0)
			force *= 3;
		      force = force / SPRINGSTRENGTH;
		      normalize (&xp, &yp, force * BALL_SPEED / object[i].M);
		      object[i].fx -= xp;
		      object[i].fy -= yp;
		      normalize (&xp, &yp, force * BALL_SPEED / object[object[i].lineto].M);
		      object[object[i].lineto].fx += xp;
		      object[object[i].lineto].fy += yp;
		    }
		}
	    }
	  if (object[i].type == ROCKET && object[i].time)
	    object[i].time--;
	  if (object[i].type == ROCKET && !object[i].time)
	    {
	      d = 640 * 640;
	      frocket = -1;
	      for (r = 0; r < nobjects; r++)
		{
		  if (object[r].live && !object[r].time && object[r].type == EHOLE)
		    {
		      int             distance;
		      float           gravity;
		      xp = object[r].x - object[i].x;
		      yp = object[r].y - object[i].y;
		      distance = sqrt (xp * xp + yp * yp);
		      gravity = BALL_SPEED * (gameplan == COOPERATIVE ? 200 : 50) / distance;
		      if (gravity > BALL_SPEED * 4 / 5)
			gravity = BALL_SPEED * 4 / 5;
		      normalize (&xp, &yp, gravity);
		      object[i].fx += xp;
		      object[i].fy += yp;
		    }
		}

	    }
	  if (object[i].type == BALL || object[i].type == LBALL || object[i].type == BBALL || object[i].type == LUNATIC)
	    {
	      frocket = -1;
	      d = 640 * 640;
	      for (r = 0; r < nrockets; r++)
		{
		  if (object[r].live && !object[r].time)
		    {
		      xp = object[r].x - object[i].x;
		      yp = object[r].y - object[i].y;
		      if (xp * xp + yp * yp < d)
			d = xp * xp + yp * yp, frocket = r;
		    }
		}
	      if (frocket != -1)
		xp = object[frocket].x - object[i].x,
		  yp = object[frocket].y - object[i].y;
	      else
		xp = GAMEWIDTH / 2 - object[i].x,
		  yp = GAMEHEIGHT / 2 - object[i].y;
	      if (object[i].type == LUNATIC && !rand () % 4)
		{
		  xp = rand ();
		  yp = rand () + 1;
		}
	      switch (object[i].type)
		{
		case BBALL:
		  normalize (&xp, &yp, BBALL_SPEED);
		  break;
		case BALL:
		case LUNATIC:
		case LBALL:
		  normalize (&xp, &yp, BALL_SPEED);
		  break;
		}
	      object[i].fx += xp;
	      object[i].fy += yp;
	    }
	  object[i].fx *= SLOWDOWN,
	    object[i].fy *= SLOWDOWN;
	}
    }
}




void
colisions ()
{
  int             i, y;
  int             colize = 0;
  static int      ctime = 0;
  float           xp, yp, gummfactor;
  for (i = 0; i < nobjects; i++)
    if (object[i].live)
      for (y = i + 1; y < nobjects; y++)
	if (object[y].live)
	  {
	    xp = object[y].x - object[i].x;
	    yp = object[y].y - object[i].y;
	    if (xp * xp + yp * yp < (object[y].radius + object[i].radius) *
		(object[y].radius + object[i].radius))
	      {
		colize = 1;
		if (object[i].type == HOLE || object[i].type == EHOLE)
		  {
		    if (object[y].type != APPLE)
		      destroy (y);
		    if (object[i].type == EHOLE)
		      destroy (i);
		    continue;
		  }
		if (object[y].type == HOLE || object[y].type == EHOLE)
		  {
		    if (object[i].type != APPLE)
		      destroy (i);
		    if (object[y].type == EHOLE)
		      destroy (y);
		    continue;
		  }
		if (object[i].type == ROCKET)
		  {
		    if (object[y].thief == 1 && object[i].thief == 1)
		      {
			float           tmp;
			tmp = object[i].M;
			object[i].M = object[y].M;
			object[y].M = tmp;
			object[i].thief = 0;
			object[y].thief = 0;
		      }
		    if (object[y].type == BBALL && object[i].thief == 1)
		      {
			object[i].M += object[y].M - M (BALL);
			object[i].thief = 0;
			object[y].M = M (BALL);
		      }
		    else if (object[y].type == ROCKET && object[i].thief == 1)
		      {
			object[i].M += object[y].M - M (ROCKET);
			object[i].accel += object[y].accel - ROCKET_SPEED;
			object[i].thief = 0;
			object[y].M = M (object[i].type);
			object[y].accel = ROCKET_SPEED - A_ADD;
		      }
		    if (object[i].type == ROCKET && object[y].thief == 1)
		      {
			object[y].M += object[i].M - M (ROCKET);
			object[y].accel += object[i].accel - ROCKET_SPEED;
			object[y].thief = 0;
			object[i].M = M (object[y].type);
			object[i].accel = ROCKET_SPEED - A_ADD;
		      }
		    if (gameplan == COOPERATIVE)
		      object[i].score++;
		    if (object[y].letter == L_ACCEL)
		      object[i].accel += A_ADD,
			object[i].score += 10;
		    if (object[y].letter == L_GUMM)
		      object[i].M += M_ADD,
			object[i].score += 10;
		    if (object[y].letter == L_THIEF)
		      object[i].M = M (object[i].type),
			object[i].accel = ROCKET_SPEED - A_ADD,
			object[i].score -= 30;
		    if (object[y].letter == L_FINDER)
		      {
			object[i].accel += A_ADD * (KRAND_N (5));
			object[i].M += M_ADD * (KRAND_N (10));
			object[i].score += 30;
		      }
		    if (object[y].letter == L_TTOOL)
		      {
			object[i].thief = 1;
			object[i].score += 30;
		      }

		    object[y].letter = ' ';
		    if (object[y].type == LBALL)
		      object[y].type = BALL;
		    if (object[y].type == BALL && dosprings && !(KRAND_N (randsprings)))
		      object[y].lineto = i;

		    if (gameplan == DEATHMATCH && object[y].type == ROCKET && dosprings && !(KRAND_N ((2 * randsprings))))
		      object[y].lineto = i;
		  }
		if (object[y].type == LUNATIC)
		  {
		    gummfactor = -ROCKETM / LUNATICM;
		  }
		else if (object[i].type == LUNATIC)
		  {
		    gummfactor = -LUNATICM / ROCKETM;
		  }
		else
		  gummfactor = object[i].M / object[y].M;
		normalize (&xp, &yp, gummfactor * GUMM);
		object[y].fx += xp;
		object[y].fy += yp;
		normalize (&xp, &yp, 1 / gummfactor * GUMM);
		object[i].fx -= xp;
		object[i].fy -= yp;
		if (object[i].type == ROCKET && object[i].time)
		  object[i].fx = 0,
		    object[i].fy = 0;
		if (object[y].type == ROCKET && object[y].time)
		  object[y].fx = 0,
		    object[y].fy = 0;
		if (object[y].type == INSPECTOR && object[i].type == ROCKET)
		  {
		    object[y].fx = 0,
		      object[y].fy = 0;
		    object[i].fx *= -2,
		      object[i].fy *= -2;
		  }
	      }
	  }
  if (colize && !ctime)
    {
#ifndef NAS_SOUND
      Effect (S_COLIZE, next);
#endif
      ctime = 4;
    }
  if (ctime)
    ctime--;
}
