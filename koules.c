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
#include <unistd.h>
#define VARIABLES_HERE
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
unsigned char   rocketcolor[5] =
{96, 160, 64, 96, 128};
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
  int             i = 0;
  while (i < npoint)
    {
      Point          *p = &point[i];
      if (--p->time <= 0)
	{
	  point[i] = point[--npoint];
	  continue;
	}
      x = (p->x += p->xp) >> 8;
      y = (p->y += p->yp);
      if (x > 0 && x < MAPWIDTH && y > 0 && y >> 8 < MAPHEIGHT)
	{
	  SMySetPixel (backscreen, x, y, p->color);
	  i++;
	}
      else
	point[i] = point[--npoint];
    }
}




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
static void
draw_objects (CONST int draw)
{
  char            s[80];
  int             i;
  if (draw)
    {
      CopyVSToVS (background, backscreen);
      SetScreen (backscreen);

      /* Now draw the objects in backscreen. */

      points ();
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
	  if (object[i].live && object[i].lineto != -1 && object[object[i].lineto].live) {
	    int             ax = FIX2I (object[i].x);
	    int             ay = FIX2I (object[i].y);
	    int             bx = FIX2I (object[object[i].lineto].x);
	    int             by = FIX2I (object[object[i].lineto].y);
	    Line (ax / DIV, ay / DIV, bx / DIV, by / DIV, 255);
	    help ((ax + bx) / 2, (ay + by) / 2, 2, "Spit");
	  }
      for (i = 0; i < nobjects; i++)
	if (object[i].live)
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
		      ox / DIV, (oy - APPLE_RADIUS + 10) / DIV, 150);
		Line ((ox + 10) / DIV + 1, (oy - APPLE_RADIUS - 10) / DIV,
		      ox / DIV + 1, (oy - APPLE_RADIUS + 10) / DIV, 150);
		if (DIV == 1)
		  Line ((ox + 10) / DIV + 2, (oy - APPLE_RADIUS - 10) / DIV,
			ox / DIV + 2, (oy - APPLE_RADIUS + 10) / DIV, 150);
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
      EnableClipping ();
      if (gameplan == COOPERATIVE && gamemode == GAME && DIV == 1)
	{
	  sprintf (s, "level: %3i", lastlevel + 1);
	  DrawWhiteMaskedText ((MAPWIDTH / 2 - 38 * 4) / 2 - strlen (s) * 4, MAPHEIGHT + 2, s);
	}
      sprintf (s, " lives: %6i%6i%6i%6i%6i",
	       nrockets >= 1 ? object[0].live1 : 0,
	       nrockets >= 2 ? object[1].live1 : 0,
	       nrockets >= 3 ? object[2].live1 : 0,
	       nrockets >= 4 ? object[3].live1 : 0,
	       nrockets >= 5 ? object[4].live1 : 0);
      DrawWhiteMaskedText (MAPWIDTH / 2 - strlen (s) * 4, MAPHEIGHT + 2, s);
      sprintf (s, "scores: %6i%6i%6i%6i%6i",
	       object[0].score,
	       object[1].score,
	       object[2].score,
	       object[3].score,
	       object[4].score);
      DrawWhiteMaskedText (MAPWIDTH / 2 - strlen (s) * 4, MAPHEIGHT + 11, s);

      /* Copy backscreen to physical screen. */
      CopyToScreen (backscreen);
      fadein ();
      DisableClipping ();
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



void
game ()
{
  long            VfTime = 0;
  long            VendSleep = 0;
  struct timeval  VlastClk;
  struct timeval  VnewClk;
  int             wait = 0;

  load_rc ();
  init_menu ();
  gettimeofday (&VlastClk, NULL);
  gettimeofday (&VnewClk, NULL);
  VendSleep = VlastClk.tv_usec;
  VfTime = 1000000 / 25;


  while (1)
    {
      process_keys ();
      sprocess_keys ();
      update_values ();
      update_game ();
      update_forces ();
      colisions ();
      move_objects ();
      check_limit ();
      gettimeofday (&VnewClk, NULL);
      if (VnewClk.tv_usec < VendSleep)
	VendSleep -= 1000000;
      wait = (VfTime - VnewClk.tv_usec + VendSleep);
      if (wait > 0 || tbreak)
	draw_objects (1);
      else
	draw_objects (0);
      gettimeofday (&VnewClk, NULL);
      if (VnewClk.tv_usec < VendSleep)
	VendSleep -= 1000000;
      wait = (VfTime - VnewClk.tv_usec + VendSleep);
      if (tbreak)
	wait = VfTime;
      if (wait > 0)
	usleep (wait);
      VendSleep = VnewClk.tv_usec + wait;
      gettimeofday (&VlastClk, NULL);
      if (tbreak)
	tbreak = 0,
	  VendSleep = VlastClk.tv_usec;

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
