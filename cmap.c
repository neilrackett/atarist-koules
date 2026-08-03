/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  C1995 JAHUSOFT                                          *
*        Jan Hubicka                                       *
*        Dukelskych Bojovniku 1944                         *
*        390 03 Tabor                                      *
*        Czech Republic                                    *
*        Phone: 0041-361-32613                             *
*        eMail: hubicka@paru.cas.cz                        *
*----------------------------------------------------------*
*   Copyright(c)1995,1996 by Jan Hubicka.See README for    *
*                     Liecnece details.                    *
*----------------------------------------------------------*
*  cmap.c  colour map                                      *
*----------------------------------------------------------*
* Upstream ran seven 32-entry ramps at bases 0/32/64/96/    *
* 128/160/192 plus white, all in simultaneous use - 224     *
* colours on screen at once.  The ST has sixteen, so each   *
* ramp collapses to two or three shades (koules.h names the *
* bases).  What is lost is hue count, not smoothness: an    *
* 8x8 ball only lights 37 pixels, and three shades read as  *
* a sphere at this size.  The casualty is five-player       *
* mode, which needs five distinguishable rocket hues plus   *
* everything else; one and two players are unaffected.      *
*                                                           *
* setcustompalette() keeps its signature and its 0..63 VGA  *
* component scale, so the fade loops below are upstream's,  *
* unchanged.                                                *
***********************************************************/

#include "koules.h"

static INLINE int
col (int p, CONST float p1)
{
  p *= p1;
  if (p > 63)
    return (63);
  if (p < 0)
    return (0);
  return (p);
}

/*
 * The sixteen entries, on upstream's 0..63 scale.  Ramps run bright
 * (specular) -> mid -> dark, matching the direction of upstream's
 * "colour + r" shading, where r is 0 at the lit point.
 *
 * Entry 0 does double duty: it is the flat playfield colour (upstream
 * randomised back(0)..back(9), which converges on this blue) and the
 * sprite colour key, which is sound because nothing inside a ball is
 * ever drawn in the background colour.
 */
static CONST unsigned char basepal[16][3] = {
  {0, 0, 18},                   /*  0 playfield / sprite key   */
  {63, 63, 63},                 /*  1 white: springs, text     */
  {63, 46, 46},                 /*  2 red bright               */
  {50, 6, 6},                   /*  3 red mid                  */
  {24, 0, 0},                   /*  4 red dark                 */
  {46, 63, 46},                 /*  5 green bright             */
  {6, 50, 6},                   /*  6 green mid                */
  {0, 24, 0},                   /*  7 green dark               */
  {46, 46, 63},                 /*  8 blue bright              */
  {12, 12, 56},                 /*  9 blue mid                 */
  {0, 0, 30},                   /* 10 blue dark                */
  {63, 63, 46},                 /* 11 yellow bright            */
  {50, 50, 6},                  /* 12 yellow mid               */
  {24, 24, 0},                  /* 13 yellow dark              */
  {56, 56, 56},                 /* 14 grey bright              */
  {28, 28, 28}                  /* 15 grey dark                */
};

int             fadedout = 0;

void
setcustompalette (CONST int p, CONST float p1)
{
  Palette         pal;
  int             i;

  for (i = 0; i < COLORS; i++)
    {
      pal.color[i].red = col (basepal[i][0] + p, p1);
      pal.color[i].green = col (basepal[i][1] + p, p1);
      pal.color[i].blue = col (basepal[i][2] + p, p1);
    }
  WaitRetrace ();
  SetPalette (&pal);
}

void
fadeout ()
{
  if (!fadedout)
    {
      float           i;
      for (i = 1; i >= 0; i -= 0.1)
	{
	  setcustompalette (0, i);
	  usleep (200), tbreak = 1;

	}
      setcustompalette (-65, 0);
      fadedout = 1;
    }
}
void
fadein ()
{
  if (fadedout)
    {
      float           i;
      for (i = 0; i <= 1; i += 0.1)
	{
	  setcustompalette (0, i);
	  usleep (200), tbreak = 1;

	}
      setcustompalette (0, 1);
      fadedout = 0;
    }
}
void
fadein1 ()			/*better for star background */
{
  if (fadedout)
    {
      int             i;
      for (i = -64; i <= 0; i += 6)
	{
	  setcustompalette (i, 1);
	  usleep (200), tbreak = 1;

	}
      setcustompalette (0, 1);
      fadedout = 0;
    }
}
