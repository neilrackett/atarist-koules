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
*  framebuffer.c fast 8 bit framebuffer bitmap creation    *
*                routines                                  *
***********************************************************/
#include "koules.h"


#define NCOLORS 32

#define HOLE_XCENTER (2*HOLE_RADIUS-3*HOLE_RADIUS/4)
#define HOLE_YCENTER (2*HOLE_RADIUS-HOLE_RADIUS/4)
#define HOLE_MAX_RADIUS (HOLE_RADIUS/DIV+0.5*HOLE_RADIUS/DIV)
#define HOLE_SIZE_MAX (radius*radius)

/*
 * Upstream shaded every sphere with 32 ramp steps; a ramp is two or
 * three entries here (see koules.h), so the 0..31 shading term is
 * rescaled rather than added raw.  r == 0 is the lit point.
 *
 * This is load-time code: the divide is a __divsi3 call on a 68000
 * but it runs a few thousand times at start-up, not in a frame.
 */
static INLINE int
ramp (CONST int base, int r)
{
  int             n = RAMPLEN (base);
  int             s;
  if (r < 0)
    r = 0;
  s = (r * n) >> 5;
  if (s >= n)
    s = n - 1;
  return base + s;
}

/*
 * The hole graphic is a small disc drawn in the top-left quadrant of
 * what upstream sized as a HOLE_RADIUS*2 square: at DIV == 2 the disc
 * is centred on (HOLE_RADIUS/DIV, HOLE_RADIUS/DIV) with radius
 * HOLE_RADIUS/4, so everything lit fits inside HOLE_RADIUS square and
 * the other three quadrants are transparent padding.  Building the
 * smaller square puts the disc in the same place on screen (PutBitmap
 * still gets (o - HOLE_RADIUS) / DIV) for a quarter of the sprite RAM
 * - 17KB of the pre-shifted set.
 */
#define HOLE_BITMAP_SIZE (HOLE_RADIUS)
#ifndef NODIRECT
/*
 * hardcoded bitmap drawing routines 
 */
static char    *
draw_ball_bitmap (int radius, CONST int color)
{
  char           *bitmap = NULL, *point;
  int             x, y, r;
  radius /= DIV;
  if ((bitmap = alloca ((radius * 2) * (radius * 2) + 2)) == NULL)
    perror ("create_ball_bitmap"), exit (-1);
  point = bitmap;
  for (y = 0; y < radius * 2; y++)
    for (x = 0; x < radius * 2; x++, point++)
      {
	if ((x - radius) * (x - radius) + (y - radius) * (y - radius)
	    < (radius - 0.5) * (radius - 0.5))
	  {
	    r = (x - 3 * radius / 4) * (x - 3 * radius / 4) +
	      (y - radius / 4) * (y - radius / 4);
	    r = r * 32 / (1.5 * radius) / (1.5 * radius);
	    if (r > 31)
	      r = 31;
	    *point = color + r;
	  }
	else
	  *point = 0;
      }
  return (CompileBitmap (radius * 2, radius * 2, (char *) bitmap));
}
static char    *
draw_reversed_ball_bitmap (int radius, CONST int color)
{
  char           *bitmap = NULL, *point;
  int             x, y, r;
  radius /= DIV;
  if ((bitmap = alloca ((radius * 2) * (radius * 2) + 2)) == NULL)
    perror ("create_ball_bitmap"), exit (-1);
  point = bitmap;
  for (y = 0; y < radius * 2; y++)
    for (x = 0; x < radius * 2; x++, point++)
      {
	if ((x - radius) * (x - radius) + (y - radius) * (y - radius)
	    < (radius - 0.5) * (radius - 0.5))
	  {
	    r = (x - 3 * radius / 4) * (x - 3 * radius / 4) +
	      (y - radius / 4) * (y - radius / 4);
	    r = r * 32 / (1.5 * radius) / (1.5 * radius);
	    if (r > 31)
	      r = 31;
	    *point = color + 16 + r / 2;
	  }
	else
	  *point = 0;
      }
  return (CompileBitmap (radius * 2, radius * 2, (char *) bitmap));
}
static char    *
draw_apple_bitmap (int radius, CONST int color)
{
  char           *bitmap = NULL, *point;
  int             x, y, r;
  int             radius1;
  radius /= DIV;
  if ((bitmap = alloca ((radius * 2) * (radius * 2) + 2)) == NULL)
    perror ("create_ball_bitmap"), exit (-1);
  point = bitmap;
  for (y = 0; y < radius * 2; y++)
    for (x = 0; x < radius * 2; x++, point++)
      {
	if (DIV == 2)
	  radius1 = radius * (abs (x - radius) / 2 + 25) / 30;
	else
	  radius1 = radius * (abs (x - radius) / 2 + 50) / 60;
	if (radius1 > radius)
	  radius1 = radius;
	if ((x - radius) * (x - radius) + (y - radius) * (y - radius)
	    < ((radius1) * (radius1)))
	  {
	    r = (x - 3 * radius / 4) * (x - 3 * radius / 4) +
	      (y - radius / 4) * (y - radius / 4);
	    r = 3 + r * 22 / (1.5 * radius1) / (1.5 * radius1);
	    if (r > 31)
	      r = 31;
	    *point = color + r;
	  }
	else
	  *point = 0;
      }
  return (CompileBitmap (radius * 2, radius * 2, (char *) bitmap));
}
#else
#ifdef XSUPPORT
static float    err = 0.0;
static INLINE int
errdist (int c)
{
  float           sat, merr;
  int             i;
  int             p;
  c &= 0xff;
  sat = opixels[c] + err;
  merr = sat - spixels[c];
  p = c;
  if (sat < spixels[c])
    {
      int             max = c / 32;
      max = (max + 1) * 32;
      for (i = c; i < max && i < 255; i++)
	{
	  if (fabs (merr) > fabs (spixels[i] - sat))
	    {
	      p = i;
	      merr = sat - spixels[i];
	    }
	}
    }
  if (sat > spixels[c])
    {
      int             min = c / 32;
      min *= 32;
      for (i = c; i > min && i > 0; i--)
	{
	  if (fabs (merr) > fabs (spixels[i] - sat))
	    {
	      p = i;
	      merr = sat - spixels[i];
	    }
	}
    }
  if (fabs (merr) > fabs (64 - sat))
    {
      p = 255;
      merr = sat - 64;
    }
  if (fabs (merr) > fabs (sat))
    {
      p = 32;
      merr = sat;
    }
  err = merr;
  return (p);
}
#else
#define errdist(c) c
#endif
/*
 * INSIDE(dx, dy, rad): upstream tested dx*dx + dy*dy < (rad-0.5)^2.
 * Multiplying both sides by four keeps it exact in integers, which
 * matters because the ST build must not drag in soft float for what
 * is a circle test.
 */
#define INSIDE(dx, dy, rad) \
  (4 * ((dx) * (dx) + (dy) * (dy)) < (2 * (rad) - 1) * (2 * (rad) - 1))

/* r * 32 / (1.5*rad) / (1.5*rad) == r * 128 / (9 * rad * rad) */
#define LIGHT(r, rad) ((r) * 128 / (9 * (rad) * (rad)))

static          BitmapType
draw_ball_bitmap (int radius, CONST int color)
{
  RawBitmapType   bbitmap;
  int             x, y, r, c;
  radius /= DIV;
  bbitmap = CreateBitmap ((radius * 2), (radius * 2));
  for (y = 0; y < radius * 2; y++)
    for (x = 0; x < radius * 2; x++)
      {
	if (INSIDE (x - radius, y - radius, radius))
	  {
	    r = (x - 3 * radius / 4) * (x - 3 * radius / 4) +
	      (y - radius / 4) * (y - radius / 4);
	    r = LIGHT (r, radius);
	    if (r > 31)
	      r = 31;
	    c = ramp (color, r);
	    BSetPixel (bbitmap, x, y, c);
	  }
	else
	  {
	    BSetPixel (bbitmap, x, y, 0);
	  }
      }
  return (CompileBitmap (radius * 2, radius * 2, bbitmap));
}
static          BitmapType
draw_reversed_ball_bitmap (int radius, CONST int color)
{
  RawBitmapType   bbitmap;
  int             x, y, r, c;
  radius /= DIV;
  bbitmap = CreateBitmap ((radius * 2), (radius * 2));
  for (y = 0; y < radius * 2; y++)
    for (x = 0; x < radius * 2; x++)
      {
	if (INSIDE (x - radius, y - radius, radius))
	  {
	    r = (x - 3 * radius / 4) * (x - 3 * radius / 4) +
	      (y - radius / 4) * (y - radius / 4);
	    r = LIGHT (r, radius);
	    if (r > 31)
	      r = 31;
	    /* upstream's "colour + 16 + r/2": the unlit half of the
	       ramp, i.e. a matt sphere rather than a shiny one */
	    c = ramp (color, 16 + r / 2);
	    BSetPixel (bbitmap, x, y, c);
	  }
	else
	  {
	    BSetPixel (bbitmap, x, y, 0);
	  }
      }
  return (CompileBitmap (radius * 2, radius * 2, bbitmap));
}
static          BitmapType
draw_apple_bitmap (int radius, CONST int color)
{
  RawBitmapType   bitmap;
  int             x, y, r, c;
  int             radius1;
  radius /= DIV;
  bitmap = CreateBitmap ((radius * 2), (radius * 2));
  for (y = 0; y < radius * 2; y++)
    for (x = 0; x < radius * 2; x++)
      {
	if (DIV == 2)
	  radius1 = radius * (abs (x - radius) / 2 + 25) / 30;
	else
	  radius1 = radius * (abs (x - radius) / 2 + 50) / 60;
	if (radius1 > radius)
	  radius1 = radius;
	if ((x - radius) * (x - radius) + (y - radius) * (y - radius)
	    < ((radius1) * (radius1)))
	  {
	    r = (x - 3 * radius / 4) * (x - 3 * radius / 4) +
	      (y - radius / 4) * (y - radius / 4);
	    /* 3 + r*22 / (1.5*r1) / (1.5*r1) */
	    r = 3 + (radius1 ? r * 88 / (9 * radius1 * radius1) : 31);
	    if (r > 31)
	      r = 31;
	    c = ramp (color, r);
	    BSetPixel (bitmap, x, y, c);
	  }
	else
	  {
	    BSetPixel (bitmap, x, y, 0);
	  }
      }
  return (CompileBitmap (radius * 2, radius * 2, bitmap));
}
#endif
void
create_bitmap ()
{
  int             x, y, r, po, radius;
#ifndef NODIRECT
  char            hole_data[HOLE_RADIUS * 2][HOLE_RADIUS * 2];
  char            ehole_data[HOLE_RADIUS * 2][HOLE_RADIUS * 2];
#else
  int             c;
  RawBitmapType   hole_data, ehole_data;
#endif
  fprintf (stderr, "creating bitmaps...\n");
#ifndef NODIRECT
  for (x = 0; x < HOLE_RADIUS * 2; x++)
    for (y = 0; y < HOLE_RADIUS * 2; y++)
      {
	if (DIV == 1)
	  radius = HOLE_RADIUS / 2 + (int) (atan (fabs (x - HOLE_RADIUS + 0.5) / fabs (y - HOLE_RADIUS + 0.5)) * HOLE_RADIUS / 2) % (HOLE_RADIUS / 2);
	else
	  radius = HOLE_RADIUS / 4;
	if ((x - HOLE_RADIUS / DIV) * (x - HOLE_RADIUS / DIV) + (y - HOLE_RADIUS / DIV) * (y - HOLE_RADIUS / DIV)
	    < radius * radius)
	  {
	    r = (x - HOLE_RADIUS / DIV) * (x - HOLE_RADIUS / DIV) +
	      (y - HOLE_RADIUS / DIV) * (y - HOLE_RADIUS / DIV);
	    r = r * 24 / HOLE_SIZE_MAX;
	    if (r > 23)
	      r = 23;
	    hole_data[x][y] = 64 + r + 1;
	    ehole_data[x][y] = 128 + r + 1;
	  }
	else
	  hole_data[x][y] = 0,
	    ehole_data[x][y] = 0;
      }
  hole_bitmap = (CompileBitmap (HOLE_RADIUS * 2, HOLE_RADIUS * 2, (char *) hole_data));
  ehole_bitmap = (CompileBitmap (HOLE_RADIUS * 2, HOLE_RADIUS * 2, (char *) ehole_data));
#else
  ehole_data = CreateBitmap (HOLE_BITMAP_SIZE, HOLE_BITMAP_SIZE);
  hole_data = CreateBitmap (HOLE_BITMAP_SIZE, HOLE_BITMAP_SIZE);
  for (x = 0; x < HOLE_BITMAP_SIZE; x++)
    for (y = 0; y < HOLE_BITMAP_SIZE; y++)
      {
	if (DIV == 1)
	  radius = HOLE_RADIUS / 2 + (int) (atan (fabs (x - HOLE_RADIUS + 0.5) / fabs (y - HOLE_RADIUS + 0.5)) * HOLE_RADIUS / 2) % (HOLE_RADIUS / 2);
	else
	  radius = HOLE_RADIUS / 4;
	if ((x - HOLE_RADIUS / DIV) * (x - HOLE_RADIUS / DIV) + (y - HOLE_RADIUS / DIV) * (y - HOLE_RADIUS / DIV)
	    < radius * radius)
	  {
	    r = (x - HOLE_RADIUS / DIV) * (x - HOLE_RADIUS / DIV) +
	      (y - HOLE_RADIUS / DIV) * (y - HOLE_RADIUS / DIV);
	    r = r * 32 / HOLE_SIZE_MAX;
	    if (r > 31)
	      r = 31;
	    c = ramp (C_RED, r);
	    BSetPixel (hole_data, x, y, c);
	    c = ramp (C_GREEN, r);
	    BSetPixel (ehole_data, x, y, c);
	  }
	else
	  {
	    BSetPixel (hole_data, x, y, 0);
	    BSetPixel (ehole_data, x, y, 0);
	  }
      }
  ehole_bitmap = (CompileBitmap (HOLE_BITMAP_SIZE, HOLE_BITMAP_SIZE, ehole_data));
  hole_bitmap = (CompileBitmap (HOLE_BITMAP_SIZE, HOLE_BITMAP_SIZE, hole_data));
#endif

  /*
   * Eyes.  Upstream gave every player their own eye ramp, starting at
   * the background ramp for player one; with two ramps' worth of grey
   * in the whole palette they are all grey here, which also keeps
   * them legible against every rocket colour.
   */
  for (po = 0; po < MAXROCKETS; po++)
    {
      eye_bitmap[po] = draw_ball_bitmap (EYE_RADIUS, C_GREY);
    }
  ball_bitmap = draw_ball_bitmap (BALL_RADIUS, ball (0));
#ifdef MOUSE
  mouse_bitmap = draw_ball_bitmap (MOUSE_RADIUS * DIV, C_GREY);
#endif
  bball_bitmap = draw_ball_bitmap (BBALL_RADIUS, C_GREEN);
  apple_bitmap = draw_apple_bitmap (APPLE_RADIUS, ball (0));
  inspector_bitmap = draw_ball_bitmap (INSPECTOR_RADIUS, C_BLUE);
  lunatic_bitmap = draw_reversed_ball_bitmap (LUNATIC_RADIUS, C_GREY);
  lball_bitmap[0] = draw_ball_bitmap (BALL_RADIUS, C_GREEN);
  lball_bitmap[1] = draw_ball_bitmap (BALL_RADIUS, C_BLUE);
  lball_bitmap[2] = draw_reversed_ball_bitmap (BALL_RADIUS, C_GREY);
  lball_bitmap[3] = draw_reversed_ball_bitmap (BALL_RADIUS, C_YELLOW);
  lball_bitmap[4] = draw_reversed_ball_bitmap (BALL_RADIUS, C_WHITE);
 /* lball_bitmap[5] = draw_reversed_ball_bitmap (BALL_RADIUS, 4 * 32 - 5);*/

  for (x = 0; x < 5; x++)
    rocket_bitmap[x] = draw_ball_bitmap (ROCKET_RADIUS, rocketcolor[x]);



}









#ifdef STDLSUPPORT
/*
 * Upstream's background is a diffusion of random values over the ten
 * darkest entries of the blue ramp, built with 64000 read-modify-write
 * pixel operations.  Both ends of that are gone here: ten shades of
 * near-black collapse to one in a sixteen colour palette (the walk
 * converges on back(9) and is clamped there, so a flat fill is what it
 * mostly was), and 64000 planar PutPixel calls would dominate start-up.
 *
 * A flat playfield is load bearing, not just cheap: it is what lets
 * the particle field be erased with one batched span fill in the
 * background colour instead of a dirty rectangle per pixel.
 */
static void
createbackground ()
{
  STDL_FillRect (background, NULL, C_BG);
}
#else
#ifndef XSUPPORT
static void
createbackground ()
{
/* Create fancy dark red background */
  int             x, y;
#ifndef NODIRECT
  char           *pixel = background->vbuf;
#endif
  for (y = 0; y < MAPHEIGHT + 20; y++)
    for (x = 0; x < MAPWIDTH; x++)
      {
	int             i = 0;
	int             n = 0;
	int             c;
	if (x > 0)
	  {
#ifndef NODIRECT
	    i += *(pixel - 1) - back (0);
#else
	    i += SGetPixel (x - 1, y) - back (0);
#endif
	    n++;
	  }
	if (y > 0)
	  {
#ifndef NODIRECT
	    i += *(pixel - MAPWIDTH) - back (0);
#else
	    i += SGetPixel (x, y - 1) - back (0);
#endif
	    n++;
	  }
	c = (i + (rand () % 16)) / (n + 1);
	if (c > 9)
	  c = 9;
#ifndef NODIRECT
	*pixel = back (0) + c;
	pixel++;
#else
	SPutPixel (x, y, back (0) + c);
#endif
      }
}
#else
#ifdef MITSHM
static void
Shmcreatebackground ()
{
/* Create fancy dark red background */
  int             x, y;
  unsigned char  *pixel = (unsigned char *) background.vbuff;
  for (y = 0; y < MAPHEIGHT + 20; y++)
    for (x = 0; x < MAPWIDTH; x++)
      {
	int             i = 0;
	int             n = 0;
	int             c;
	if (x > 0)
	  {
	    i += *(pixel - 1) - back (0);
	    n++;
	  }
	if (y > 0)
	  {
	    i += *(pixel - MAPWIDTH) - back (0);
	    n++;
	  }
	c = (i + (rand () % 16)) / (n + 1);
	if (c > 9)
	  c = 9;
	*pixel = back (0) + c;
	pixel++;
      }
  pixel = (unsigned char *) background.vbuff;
  for (y = 0; y < MAPHEIGHT + 20; y++)
    for (x = 0; x < MAPWIDTH; x++)
      *pixel = (unsigned char) pixels[*pixel],
	pixel++;
}
#endif
static void
createbackground ()
{
/* Create fancy dark red background */
  int             x, y;
  XImage         *img;
  char           *data;
#ifdef MITSHM
  if (shm)
    {
      Shmcreatebackground ();
      return;
    }
#endif
  if ((data = (char *) calloc ((MAPWIDTH + BitmapPad (dp)) * (MAPHEIGHT + 20) * 4, 1)) == NULL)
    perror ("Memory Error"), exit (2);
  img = XCreateImage (dp, DefaultVisual (dp, screen), DefaultDepth (dp, screen),
		      ZPixmap, 0, data, MAPWIDTH, MAPHEIGHT + 20,
		      BitmapPad (dp), 0);
  for (y = 0; y < MAPHEIGHT + 20; y++)
    for (x = 0; x < MAPWIDTH; x++)
      {
	int             i = 0;
	int             n = 0;
	int             c;
	if (x > 0)
	  {
	    i += XGetPixel (img, x - 1, y);
	    n++;
	  }
	if (y > 0)
	  {
	    i += XGetPixel (img, x, y - 1);
	    n++;
	  }
	c = (i + (rand () % 16)) / (n + 1);
	if (c > 9)
	  c = 9;
	/*gl_setpixel (x, y, back (0) + c); */
	XPutPixel (img, x, y, c & 0xff);
      }
  for (y = 0; y < MAPHEIGHT + 20; y++)
    for (x = 0; x < MAPWIDTH; x++)
      XPutPixel (img, x, y, pixels[back (0) + XGetPixel (img, x, y)]);
  XPutImage (dp, current.pixmap, gc, img, 0, 0, 0, 0, MAPWIDTH, MAPHEIGHT + 20);
  XSync (dp, 1);
  XDestroyImage (img);
}
#endif
#endif /* STDLSUPPORT */
void
drawstarbackground ()
{
#ifdef STDLSUPPORT
  /* No persistent star surface on the ST: it is 32KB that only the
     scroller used, and stdl/intro.c scatters its stars straight onto
     the screen instead. */
#else
/* Create fancy dark red background */
  int             x;
  int             x1, y1, c1;
  SetScreen (starbackground);
  ClearScreen ();
  for (x = 0; x < 700 / DIV / DIV; x++)
    {
      x1 = rand () % MAPWIDTH;
      y1 = rand () % (MAPHEIGHT + 20);
      c1 = rand () % 32 + 192;
      SSetPixel (x1, y1, c1);
    }
#endif
}




void
drawbackground ()
{				/*int i; */
/* Build up background from map data */
  SetScreen (background);
  ClearScreen ();
  createbackground ();
  EnableClipping ();
#ifdef MITSHM
  if (shm)
    HLine (0, MAPHEIGHT, MAPWIDTH - 1, back (16));
  else
#endif
    Line (0, MAPHEIGHT, MAPWIDTH - 1, MAPHEIGHT, back (16));
  DisableClipping ();
}
