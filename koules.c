/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  C1995 JAHUSOFT                                          *
*        Jan Hubicka                                       *
*        Dukelskych Bojovniku 1944                         *
*        390 03 Tabor                                      *
*        Czech Republic                                    *
*        Phone: 0041-361-32613                             *
*        eMail: hubicka@limax.paru.cas.cz                  *
*----------------------------------------------------------*
*   Copyright(c)1995,1996 by Jan Hubicka.See README for    *
*                    licence details.                      *
*----------------------------------------------------------*
*  koules.c main game routines                             *
***********************************************************/
/* Changes for OS/2 Warp with Dive.                        *
 *  Copyright(c)1996 by Thomas A. K. Kjaer                 *
 ***********************************************************/
/* Changes for joystick "accelerate by deflection"         *
 *  (c) 1997 by Ludvik Tesar (Ludv\'{\i}k Tesa\v{r})       *
 ************************LT*********************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/
#include <unistd.h>
/*
 * Phase 1 moved object[], point[], nobjects and nrockets into
 * physics.c so the simulation could link without a backend, which
 * left koules.c defining VARIABLES_HERE (= "do not declare the
 * globals, I own them") while no longer owning all of them.  Take
 * the declarations from the header like every other file; the few
 * globals koules.c does define below are then definitions matching
 * an extern, which is what C wants anyway.
 */
#include "koules.h"
#include "physics.h"
#ifdef NETSUPPORT
#include "server.h"
#include "client.h"
#endif
#include <sys/time.h>
int             drawpointer = 1;
int             difficulty = 2;
int             cit = 0;
#ifdef NETSUPPORT
int             client = 0, server = 0;
#endif


BitmapType      bball_bitmap, apple_bitmap, inspector_bitmap, mouse_bitmap,
                lunatic_bitmap, lball_bitmap[NLETTERS], circle_bitmap,
                hole_bitmap, ball_bitmap, eye_bitmap[MAXROCKETS], rocket_bitmap[MAXROCKETS],
                ehole_bitmap;
/* One ramp base per player. Upstream repeated yellow for players 1
 * and 4; there are exactly five ramps here, so each gets its own. */
unsigned char   rocketcolor[5] =
{C_YELLOW, C_BLUE, C_RED, C_GREEN, C_GREY};
#ifdef SOUND
int             sndinit = 1;
#endif

int             lastlevel = 0, maxlevel = 0;
unsigned char   control[MAXROCKETS];
struct control  controls[MAXROCKETS];
int             nomouse = 0;
int             textcolor;
int             sound = 1;
int             tbreak;
int             gameplan = COOPERATIVE;
int             gamemode;
int             keys[5][4];
int             rotation[MAXROCKETS];
#ifdef MOUSE
int             mouseplayer = -1;
#endif
#ifdef JOYSTICK
int             joystickplayer[2] =
{-1, -1};
int             joystickdevice[2] =
{-1, -1};
int             calibrated[2];
int             center[2][2];
float    joystickmul[2]={1.5,1.5};
float    joystickthresh[2]={0.1,0.1};
#endif





/*
 * Step and draw the particles.  Same compaction as points1() in
 * physics.c: point[0..npoint) are live and a dead one is replaced
 * by the last.  Leaving the playfield kills a particle, which
 * upstream signalled by setting time=0 and then re-walking the
 * slot every frame until the sweep noticed.
 *
 * (The X11 and MIT-SHM fast paths that used to live here went with
 * xlib/ -- there is no X server on a 520ST.)
 */
void
points (void)
{
  unsigned int    x, y;
  Point          *p = point;
  Point          *last = point + npoint;

  /* Walking pointers, not indices: Point is 24 bytes, so gcc 4.6
     turns every point[i] into a shift-and-add chain plus a 32 bit
     add of the array base - about a sixth of the whole step.  The
     dead-particle swap still indexes, but it only runs for the few
     particles that die in a frame. */
  while (p < last)
    {
      if (--p->time <= 0)
	{
	  *p = *--last;
	  continue;
	}
      x = (unsigned int) (p->x += p->xp) >> 8;
      y = (unsigned int) (p->y += p->yp);
      if (x > 0 && x < MAPWIDTH && y > 0 && y >> 8 < MAPHEIGHT)
	{
	  SMySetPixel (backscreen, x, y, p->color);
	  p++;
	}
      else
	*p = *--last;
    }
  npoint = (int) (last - point);
}




/*
 * Frame-rate readout, shown in the status bar and toggled with F.
 * Kept in the shipped build on purpose: the port's whole question is
 * what it costs per frame, and a screenshot that carries the answer
 * is worth more than a console line the ST cannot print without
 * scribbling on the playfield.
 */
char            profiletext[32];
int             profilestamp;   /* bumped when profiletext changes */
static int      profileon = 1;
extern int      menuchanged;
static int      lastmode = -1;

static int helpmode;
static void help(int x,int y,int radius,char *text)
{
   int x1=x+radius+2,y1=y-4*DIV,x2=x1+strlen(text)*8*DIV,y2=y1+8*DIV;
   if(helpmode&&x1>0&&x2<=GAMEWIDTH-DIV&&y1>0&&y2<GAMEHEIGHT-DIV) {
   DrawBlackMaskedText(x1/DIV+1,y1/DIV+1,text);
   DrawWhiteMaskedText(x1/DIV,y1/DIV,text);
   }
}
char            str[2];
#ifdef KOULES_DEBUG
uint32_t        ph_restore, ph_points, ph_obj, ph_over, ph_stat, ph_step;
uint32_t        ph_pump;        /* event pump, incl. the sound refill */
#define PHASE(acc) do { uint32_t now = STDL_GetTicks (); \
                        acc += now - phmark; phmark = now; } while (0)
#else
#define PHASE(acc) do { } while (0)
#endif
static void
draw_objects (CONST int draw)
{
  char            s[80];
  int             i;
#ifdef KOULES_DEBUG
  uint32_t        phmark = STDL_GetTicks ();
#endif
  if (draw)
    {
      /*
       * Upstream repainted the whole background surface over the
       * whole back screen here - 90kpx for the 2-10kpx that actually
       * move.  Instead every draw that lands on the screen records
       * its bounding box (see stdl/draw.c) and only those boxes are
       * repainted.  Particles are erased first, wholesale, because
       * they are single pixels: a rectangle each would cost more
       * than the pixel does.
       */
      ErasePoints ();
      RestoreBackground ();
      SetScreen (backscreen);
      PHASE (ph_restore);

      /* Now draw the objects on the screen. */

      if (gamemode == GAME)
	points ();
      else
	points1 ();             /* menu up: step, do not draw */
      PHASE (ph_step);
      FlushPoints ();
      PHASE (ph_points);
      help(0,9,0,"Help - press 'H' to disable");
#ifdef XSUPPORT
#ifdef MITSHM
      if (!shm)
#else
      if (1)
#endif
	{
	  XSegment        lines[MAXOBJECT];
	  int             nlines = 0;
	  for (i = 0; i < nobjects; i++)
	    if (object[i].live && object[i].lineto != -1 && object[object[i].lineto].live)
	      {
		lines[nlines].x1 = ox / DIV;
		lines[nlines].y1 = oy / DIV;
		lines[nlines].x2 = object[object[i].lineto].x / DIV;
		lines[nlines].y2 = object[object[i].lineto].y / DIV;
		nlines++;
	      }
	  SetColor (255);
	  XDrawSegments (dp, current.pixmap, gc, lines, nlines);

	}
      else
#endif
	for (i = 0; i < nobjects; i++)
	  if (gamemode == GAME && object[i].live && object[i].lineto != -1
	      && object[object[i].lineto].live) {
	    int             ax = FIX2I (object[i].x);
	    int             ay = FIX2I (object[i].y);
	    int             bx = FIX2I (object[object[i].lineto].x);
	    int             by = FIX2I (object[object[i].lineto].y);
	    Line (ax / DIV, ay / DIV, bx / DIV, by / DIV, C_WHITE);
	    help ((ax + bx) / 2, (ay + by) / 2, 2, "Spit");
	  }
      for (i = 0; i < nobjects; i++)
	if (object[i].live && gamemode == GAME)
	  {
	    /* one shift each instead of a conversion per use */
	    CONST int       ox = FIX2I (object[i].x);
	    CONST int       oy = FIX2I (object[i].y);

	    switch (object[i].type)
	      {
	      case BALL:
		PutBitmap ((ox - BALL_RADIUS) / DIV, (oy - BALL_RADIUS) / DIV,
		 BALL_RADIUS * 2 / DIV, BALL_RADIUS * 2 / DIV, ball_bitmap);
		help(ox,oy,object[i].radius,"Koules");
		break;
	      case LBALL:
		switch (object[i].letter)
		  {
		  case L_ACCEL:
		    PutBitmap ((ox - BALL_RADIUS) / DIV, (oy - BALL_RADIUS) / DIV,
			       BALL_RADIUS * 2 / DIV, BALL_RADIUS * 2 / DIV, lball_bitmap[0]);
		    help(ox,oy,object[i].radius,"Acceleration");
		    break;
		  case L_GUMM:
		    PutBitmap ((ox - BALL_RADIUS) / DIV, (oy - BALL_RADIUS) / DIV,
			       BALL_RADIUS * 2 / DIV, BALL_RADIUS * 2 / DIV, lball_bitmap[1]);
		    help(ox,oy,object[i].radius,"Weight");
		    break;
		  case L_THIEF:
		    PutBitmap ((ox - BALL_RADIUS) / DIV, (oy - BALL_RADIUS) / DIV,
			       BALL_RADIUS * 2 / DIV, BALL_RADIUS * 2 / DIV, lball_bitmap[2]);
		    help(ox,oy,object[i].radius,"Thief");
		    break;
		  case L_FINDER:
		    PutBitmap ((ox - BALL_RADIUS) / DIV, (oy - BALL_RADIUS) / DIV,
			       BALL_RADIUS * 2 / DIV, BALL_RADIUS * 2 / DIV, lball_bitmap[3]);
		    help(ox,oy,object[i].radius,"Goodie");
		    break;
		  case L_TTOOL:
		    PutBitmap ((ox - BALL_RADIUS) / DIV, (oy - BALL_RADIUS) / DIV,
			       BALL_RADIUS * 2 / DIV, BALL_RADIUS * 2 / DIV, lball_bitmap[4]);
		    help(ox,oy,object[i].radius,"Thief toolkit");
		    break;
		  }
#if !defined(XSUPPORT)||defined(MITSHM)
#ifdef MITSHM
		if (DIV == 1 && shm)
#else
		if (DIV == 1)
#endif
		  {
		    str[0] = object[i].letter;
		    DrawBlackMaskedText (ox / DIV - 4, oy / DIV - 4, str);
		  }
#endif
		break;
	      case HOLE:
		EnableClipping ();
		PutBitmap ((ox - HOLE_RADIUS) / DIV, (oy - HOLE_RADIUS) / DIV,
			   HOLE_RADIUS * 2, HOLE_RADIUS * 2, hole_bitmap);
		DisableClipping ();
		help(ox,oy,object[i].radius,"Black hole");
		break;
	      case EHOLE:
		EnableClipping ();
		PutBitmap ((ox - HOLE_RADIUS) / DIV, (oy - HOLE_RADIUS) / DIV,
			   HOLE_RADIUS * 2, HOLE_RADIUS * 2, ehole_bitmap);
		DisableClipping ();
		help(ox,oy,object[i].radius,"Magnetic hole");
		break;
	      case BBALL:
		PutBitmap ((ox - BBALL_RADIUS) / DIV, (oy - BBALL_RADIUS) / DIV,
			   BBALL_RADIUS * 2 / DIV, BBALL_RADIUS * 2 / DIV, bball_bitmap);
		help(ox,oy,object[i].radius,"BBALL!");
		break;
	      case INSPECTOR:
		PutBitmap ((ox - INSPECTOR_RADIUS) / DIV, (oy - INSPECTOR_RADIUS) / DIV,
			   INSPECTOR_RADIUS * 2 / DIV, INSPECTOR_RADIUS * 2 / DIV, inspector_bitmap);
		help(ox,oy,object[i].radius,"Inspector");
		break;
	      case LUNATIC:
		PutBitmap ((ox - LUNATIC_RADIUS) / DIV, (oy - LUNATIC_RADIUS) / DIV,
			   LUNATIC_RADIUS * 2 / DIV, LUNATIC_RADIUS * 2 / DIV, lunatic_bitmap);
		help(ox,oy,object[i].radius,"Lunatic");
		break;
	      case APPLE:
		PutBitmap ((ox - APPLE_RADIUS) / DIV, (oy - APPLE_RADIUS) / DIV,
			   APPLE_RADIUS * 2 / DIV, APPLE_RADIUS * 2 / DIV, apple_bitmap);
		EnableClipping ();
		Line ((ox + 10) / DIV, (oy - APPLE_RADIUS - 10) / DIV,
		      ox / DIV, (oy - APPLE_RADIUS + 10) / DIV, C_GREEN + 1);
		Line ((ox + 10) / DIV + 1, (oy - APPLE_RADIUS - 10) / DIV,
		      ox / DIV + 1, (oy - APPLE_RADIUS + 10) / DIV, C_GREEN + 1);
		if (DIV == 1)
		  Line ((ox + 10) / DIV + 2, (oy - APPLE_RADIUS - 10) / DIV,
			ox / DIV + 2, (oy - APPLE_RADIUS + 10) / DIV, C_GREEN + 1);
		DisableClipping ();
		PutBitmap ((ox - EYE_RADIUS) / DIV,
			   (oy + APPLE_RADIUS - 15) / DIV,
		 EYE_RADIUS * 2 / DIV, EYE_RADIUS * 2 / DIV, eye_bitmap[0]);
		help(ox,oy,object[i].radius,"APPLEPOLISHER");
		break;
	      case ROCKET:
		{
		  int             x1, y1;
		help(ox,oy,object[i].radius,"Player");
		  PutBitmap ((ox - ROCKET_RADIUS) / DIV, (oy - ROCKET_RADIUS) / DIV,
			     ROCKET_RADIUS * 2 / DIV, ROCKET_RADIUS * 2 / DIV, rocket_bitmap[i]);
		  EnableClipping ();
		  if (!object[i].thief)
		    {
		      x1 = ox + FIX2I (fixsin (object[i].rotation - RAD (30)) * EYE_RADIUS1) - EYE_RADIUS;
		      y1 = oy + FIX2I (fixcos (object[i].rotation - RAD (30)) * EYE_RADIUS1) - EYE_RADIUS;
		      PutBitmap ((int) (x1 / DIV), (int) (y1 / DIV),
				 (int) (EYE_RADIUS * 2 / DIV), (EYE_RADIUS * 2 / DIV), eye_bitmap[i]);
		      x1 = ox + FIX2I (fixsin (object[i].rotation + RAD (30)) * EYE_RADIUS1) - EYE_RADIUS;
		      y1 = oy + FIX2I (fixcos (object[i].rotation + RAD (30)) * EYE_RADIUS1) - EYE_RADIUS;
		      PutBitmap ((int) (x1 / DIV), (int) (y1 / DIV),
				 (int) (EYE_RADIUS * 2 / DIV), (EYE_RADIUS * 2 / DIV), eye_bitmap[i]);
		    }
		  else
		    {
		      x1 = ox + FIX2I (fixsin (object[i].rotation - RAD (30)) * EYE_RADIUS1) - BALL_RADIUS;
		      y1 = oy + FIX2I (fixcos (object[i].rotation - RAD (30)) * EYE_RADIUS1) - BALL_RADIUS;

		      PutBitmap ((int) (x1 / DIV), (int) (y1 / DIV),
				 (int) (BALL_RADIUS * 2 / DIV), (BALL_RADIUS * 2 / DIV), lball_bitmap[2]);
		      x1 = ox + FIX2I (fixsin (object[i].rotation + RAD (30)) * EYE_RADIUS1) - BALL_RADIUS;
		      y1 = oy + FIX2I (fixcos (object[i].rotation + RAD (30)) * EYE_RADIUS1) - BALL_RADIUS;
		      PutBitmap ((int) (x1 / DIV), (int) (y1 / DIV),
				 (int) (BALL_RADIUS * 2 / DIV), (BALL_RADIUS * 2 / DIV), lball_bitmap[2]);
		    }
		  DisableClipping ();
		}
		break;
	      }
	  }
    }
  /*if draw */
  else
    points1 ();
  PHASE (ph_obj);
  /*
   * Overlays.  Painting one is expensive (hundreds of glyphs) and
   * pointless when nothing about it has moved, so it goes on screen
   * without being recorded as dirty - the next restore then leaves
   * it standing - and is repainted only when menu.c says its content
   * changed.  The mode functions still run every frame with draw=0
   * so their animation timers keep ticking.
   */
  if (lastmode != gamemode)
    lastmode = gamemode, menuchanged = 1;
  if (gamemode == MENU || gamemode == KEYS || gamemode == JOY)
    {
      int             paint = draw && menuchanged;
      if (paint)
	{
	  ClearOverlay ();
	  SuppressDirty (1);
	}
      switch (gamemode)
	{
	case MENU:
	  draw_menu (paint);
	  break;
	case KEYS:
	  draw_keys (paint);
	  break;
#ifdef JOYSTICK
	case JOY:
	  draw_joy (paint);
	  break;
#endif
	}
      if (paint)
	{
	  SuppressDirty (0);
	  menuchanged = 0;
	}
    }

#ifdef MOUSE
  if (draw && (gamemode == MENU || (gamemode == GAME && mouseplayer != -1)) &&
      MouseX () >= 0 && MouseY () >= 0 && MouseX () < MAPWIDTH &&
      MouseY () < MAPHEIGHT && drawpointer)
    {
      EnableClipping ();
      if (!nomouse)
	{
	  PutBitmap (MouseX () - MOUSE_RADIUS, MouseY () - MOUSE_RADIUS,
		     MOUSE_RADIUS * 2, MOUSE_RADIUS * 2, mouse_bitmap);
	  DisableClipping ();
	}
    }
#endif
  if (draw)
    {
      /*
       * The status bar is the one part of the screen nothing else
       * draws on - no object reaches y >= MAPHEIGHT and neither does
       * a particle - so it survives untouched between frames.  Only
       * repaint it when the numbers change: upstream redrew four
       * shadowed strings every frame, which is precisely the cost
       * that hurt the Sopwith port.
       */
      /*
       * Only format when something changed.  mintlib's sprintf costs
       * around 10ms for these two lines on an 8MHz 68000 - as much as
       * drawing every object in the level - and the numbers change a
       * few times a minute.
       */
      static int      lastsig[9] = { -1 };
      static char     s2[80];
      int             sig[9];
      int             k, same = 1;

      sig[0] = lastlevel;
      for (k = 0; k < 5; k++)
	sig[1 + k] = (nrockets > k ? object[k].live1 : 0);
      sig[6] = object[0].score;
      sig[7] = object[1].score;
      sig[8] = profilestamp;
      for (k = 0; k < 9; k++)
	if (sig[k] != lastsig[k])
	  same = 0, lastsig[k] = sig[k];
      PHASE (ph_over);
      if (!same)
	{
	  sprintf (s, "level %i   lives%4i%4i%4i%4i%4i", lastlevel + 1,
		   sig[1], sig[2], sig[3], sig[4], sig[5]);
	  sprintf (s2, "score %i", object[0].score);
	  if (nrockets > 1)
	    sprintf (s2 + strlen (s2), " %i", object[1].score);
	  if (profiletext[0])
	    sprintf (s2 + strlen (s2), "  %s", profiletext);
	  StatusBar (s, s2);
	}
      PHASE (ph_stat);

      /* Nothing to copy: the game draws into screen memory. */
      CopyToScreen (backscreen);
      fadein ();
    }
}


static void
sprocess_keys ()
{
  int             i;
  if (gamemode != GAME)
    return;
  for (i = 0; i < MAXROCKETS; i++)
    {
      if (object[i].live && object[i].type == ROCKET)
	{
	  switch (controls[i].type)
	    {
#ifdef JOYSTICK
	    case C_JOYSTICK1:
	      {
		double          a, x = controls[i].jx, y = controls[i].jy;
		a = atan (fabs (y) / fabs (x));
		if (x < 0 && y >= 0)
		  object[i].rotation = a + RAD (90);
		else if (x < 0 && y < 0)
		  object[i].rotation = RAD (90) - a;
		else if (x >= 0 && y < 0)
		  object[i].rotation = a + RAD (270);
		else if (x >= 0 && y >= 0)
		  object[i].rotation = RAD (270) - a;
		/* Measure the deflection (a is betw. 0 and 1) */
  	        a=hypot(x*object[i].joymulx,y*object[i].joymuly);
           	/* I must make sure, that I am not cheating :-)  */
		/* "a" can't be bigger than one */
	        if((a>1.0)||(controls[i].mask!=0))a=1.0;
	        if(a>object[i].joythresh)accel(i,a);
	      }
	      break;
#endif
#ifdef MOUSE
	    case C_MOUSE:
	      {
		double          dx, dy, a;
		dx = ox - controls[i].mx;
		dy = oy - controls[i].my;
		if (dx == 0)
		  dx = 0.001;
		a = atan (fabs (dy) / fabs (dx));
		if (dx < 0 && dy >= 0)
		  object[i].rotation = a + RAD (90);
		else if (dx < 0 && dy < 0)
		  object[i].rotation = RAD (90) - a;
		else if (dx >= 0 && dy < 0)
		  object[i].rotation = a + RAD (270);
		else if (dx >= 0 && dy >= 0)
		  object[i].rotation = RAD (270) - a;
		if (controls[i].mask)
		  accel (i, OVAL (1.0));
	      }
	      break;
#endif
	    case C_RKEYBOARD:
	      if (controls[i].mask & 1)
		object[i].rotation += ROTSTEP;
	      if (controls[i].mask & 2)
		object[i].rotation -= ROTSTEP;
#ifndef KOULES_FLOAT
	      ANGWRAP (object[i].rotation);
#endif
	      if (controls[i].mask & 4)
		accel (i, OVAL (1.0));
	      break;
	    case C_KEYBOARD:
	      switch (controls[i].mask)
		{
		case 1:
		  object[i].rotation = RAD (-135), accel (i, OVAL (1.0));
		  break;
		case 2:
		  object[i].rotation = RAD (135), accel (i, OVAL (1.0));
		  break;
		case 3:
		  object[i].rotation = RAD (45), accel (i, OVAL (1.0));
		  break;
		case 4:
		  object[i].rotation = RAD (-45), accel (i, OVAL (1.0));
		  break;
		case 5:
		  object[i].rotation = RAD (-90), accel (i, OVAL (1.0));
		  break;
		case 6:
		  object[i].rotation = RAD (90), accel (i, OVAL (1.0));
		  break;
		case 7:
		  object[i].rotation = RAD (180), accel (i, OVAL (1.0));
		  break;
		case 8:
		  object[i].rotation = RAD (0), accel (i, OVAL (1.0));
		  break;
		}

	    }
	}
    }
}

void
process_keys ()
{
  int             i;
  static int lasth=0;
#ifdef JOYSTICK
  int             status;
  struct JS_DATA_TYPE js;
#endif


  UpdateInput ();
  if (IsPressedH () && !lasth)
    {
      helpmode^=1;
    }
  lasth=IsPressedH();
  if (IsPressedP () && !client)
    {
      int             k = 1;
      SetScreen (backscreen);
      DrawText (MAPWIDTH / 2 - 20, MAPHEIGHT / 2 - 4, "PAUSE");
      CopyToScreen(backscreen);
#ifdef OS2DIVE
      forceBlitting ();
#endif
      tbreak = 1;
      while (k)
	{
	  UpdateInput ();
	  k = Pressed ();
#ifdef OS2DIVE
	  DosSleep (WAIT);
#endif
	}
      while (!k)
	{
	  UpdateInput ();
	  k = Pressed ();
#ifdef OS2DIVE
	  DosSleep (WAIT);
#endif
	}
    }
  switch (gamemode)
    {
    case MENU:
      menu_keys ();
      break;
    case KEYS:
      keys_keys ();
      break;
#ifdef JOYSTICK
    case JOY:
      joy_keys ();
      break;
#endif
    case GAME:
#ifdef JOYSTICK
      for (i = 0; i < 2; i++)
	{
	  double          x, y;
	  if (joystickplayer[i] >= 0)
	    {
	      if (object[joystickplayer[i]].type != ROCKET)
		continue;
	      status = read (joystickdevice[i], &js, JS_RETURN);
	      if (status != JS_RETURN)
		{
		  break;
		}
	      x = center[i][0] - js.x;
	      y = center[i][1] - js.y;	       
              if (x == 0)
		x = 0.001;
	      controls[joystickplayer[i]].jx = x;
	      controls[joystickplayer[i]].jy = y;
	      controls[joystickplayer[i]].mask = js.buttons;
	      controls[joystickplayer[i]].type = C_JOYSTICK1;

	    }
	}
#endif
#ifdef MOUSE
      /* Move. */
      if (mouseplayer != -1 && object[mouseplayer].type == ROCKET
      /*&& (MouseButtons ()||controls[mouseplayer].mask) */ )
	{
	  controls[mouseplayer].mx = MouseX () * DIV;
	  controls[mouseplayer].my = MouseY () * DIV;
	  controls[mouseplayer].mask = MouseButtons () != 0;
	  controls[mouseplayer].type = C_MOUSE;
	}
#endif
      if (IsPressedEsc ())
	{
#ifdef NETSUPPORT
	  if (!client)
	    {
#endif
	      gamemode = MENU;
	      while (IsPressedEsc ())
		UpdateInput ();
#ifdef NETSUPPORT
	    }
	  else
	    {
	      CQuit ("client exit-ESC pressed\n");
	    }
#endif
	}
      for (i = 0; i < nrockets; i++)
	{
#ifdef MOUSE
	  if (i == mouseplayer)
	    continue;
#endif
#ifdef JOYSTICK
	  if (i == joystickplayer[0] ||
	      i == joystickplayer[1])
	    continue;
#endif
#ifdef NETSUPPORT
	  if (client && !control[i])
	    continue;
#endif
	  if (object[i].type != ROCKET)
	    continue;
	  if (rotation[i])
	    {
	      char            s = 0;
	      if (IsPressed (keys[i][1]))
		s = 1;
	      if (IsPressed (keys[i][2]))
		s |= 2;
	      if (IsPressed (keys[i][0]))
		s |= 4;
	      controls[i].type = C_RKEYBOARD;
	      controls[i].mask = s;
	    }
	  else
	    {
	      int             s = 0;
	      if (IsPressed (keys[i][2]) && IsPressed (keys[i][0]))
		s = 1;
	      else if (IsPressed (keys[i][3]) && IsPressed (keys[i][0]))
		s = 2;
	      else if (IsPressed (keys[i][1]) && IsPressed (keys[i][3]))
		s = 3;
	      else if (IsPressed (keys[i][1]) && IsPressed (keys[i][2]))
		s = 4;
	      else if (IsPressed (keys[i][2]))
		s = 5;
	      else if (IsPressed (keys[i][3]))
		s = 6;
	      else if (IsPressed (keys[i][0]))
		s = 7;
	      else if (IsPressed (keys[i][1]))
		s = 8;
	      controls[i].type = C_KEYBOARD;
	      controls[i].mask = s;
	    }
	}
      break;
    }



}



/*
 * Frame pacing.  Upstream tracked struct timeval microseconds within
 * the current second, which needs a 1MHz clock the ST has not got:
 * STDL's is the 200Hz system timer, so everything here is whole
 * milliseconds and the deadline is absolute rather than rebuilt from
 * tv_usec each frame.  The behaviour is upstream's: simulate every
 * frame, draw only when the frame still has time left in it.
 */
#define VFTIME (1000 / 25)      /* 40ms - upstream's 1000000/25 */

void
game ()
{
  uint32_t        deadline;
  uint32_t        statwin;      /* start of the current 1s window   */
  uint32_t        physms = 0, drawms = 0;
  int             drawn = 0, skipped = 0;
  int             skippedlast = 0;
  static int      lastf = 0;

  load_rc ();
  init_menu ();
  deadline = STDL_GetTicks ();
  statwin = deadline;

  while (1)
    {
      uint32_t        t0, t1, t2;

      t0 = STDL_GetTicks ();
      deadline += VFTIME;       /* when this frame should be over */
      process_keys ();
      sprocess_keys ();
      update_values ();
      update_game ();
      update_forces ();
      colisions ();
      move_objects ();
      check_limit ();
      t1 = STDL_GetTicks ();

      /*
       * Upstream draws only while the frame still has budget left,
       * which on hardware this slow means never: the simulation alone
       * runs over the 40ms budget from about a dozen objects up, so a
       * plain "am I behind" test locks the picture solid (measured -
       * one frame drawn at start-up, then nothing ever again).
       * Skipping is still worth having when the simulation overruns,
       * so it is capped at every other frame - the picture can halve,
       * never stop - with half a frame of slack before the first
       * drop, because the 200Hz clock has 5ms granularity and
       * STDL_Delay rounds up, which on its own threw away one frame
       * in six for nothing.
       */
      if ((int32_t) (t1 - deadline) < VFTIME / 2 || tbreak || skippedlast)
	draw_objects (1), drawn++, skippedlast = 0;
      else
	draw_objects (0), skipped++, skippedlast = 1;
      t2 = STDL_GetTicks ();

      physms += t1 - t0;
      drawms += t2 - t1;

      if (IsPressed (STDLK_f))
	{
	  if (!lastf)
	    {
	      profileon ^= 1;
	      if (!profileon)
		profiletext[0] = 0;
	    }
	  lastf = 1;
	}
      else
	lastf = 0;

      if (t2 - statwin >= 1000)
	{
	  int             n = drawn + skipped;
	  /* frames drawn, mean simulation ms, mean render ms */
	  if (profileon && n)
	    {
	      sprintf (profiletext, "%if %ip %id", drawn,
		       (int) (physms / n),
		       (int) (drawms / (drawn ? drawn : 1)));
	      profilestamp++;
	    }
#ifdef KOULES_DEBUG
	  fprintf (stderr,
		   "d=%d s=%d phys=%lu draw=%lu | rest=%lu pts=%lu obj=%lu"
		   " over=%lu stat=%lu step=%lu pump=%lu np=%d mode=%d\n",
		   drawn, skipped, (unsigned long) physms,
		   (unsigned long) drawms, (unsigned long) ph_restore,
		   (unsigned long) ph_points, (unsigned long) ph_obj,
		   (unsigned long) ph_over, (unsigned long) ph_stat,
		   (unsigned long) ph_step, (unsigned long) ph_pump,
		   npoint, gamemode);
	  ph_restore = ph_points = ph_obj = ph_over = ph_stat = 0;
	  ph_step = ph_pump = 0;
#endif
	  statwin = t2;
	  physms = drawms = 0;
	  drawn = skipped = 0;
	}

      if (tbreak)
	{
	  /* a fade, a level change or a briefing just ate an
	     unbounded amount of wall clock: start the frame clock and
	     the statistics window again rather than trying to catch
	     up on time that was never ours */
	  tbreak = 0;
	  deadline = t2;
	  statwin = t2;
	  physms = drawms = 0;
	  drawn = skipped = 0;
	  skippedlast = 0;
	  continue;
	}
      if ((int32_t) (deadline - t2) > 0)
	STDL_Delay (deadline - t2);
      else if ((int32_t) (t2 - deadline) > 4 * VFTIME)
	deadline = t2;          /* fell far behind: resynchronise */
    }
}
#ifdef NETSUPPORT
void
client_loop2 (int draw)		/*game part of server loop */
{
  draw_objects (draw);
  switch (gamemode)
    {
    case MENU:
      draw_menu (draw);
      break;
    case KEYS:
      draw_keys (draw);
      break;
#ifdef JOYSTICK
    case JOY:
      draw_joy (draw);
      break;
#endif
    }
}
void
server_loop2 (void)		/*game part of server loop */
{
  sprocess_keys ();
  update_values ();
  update_game ();
  update_forces ();
  colisions ();
  move_objects ();
  check_limit ();
}
#endif
