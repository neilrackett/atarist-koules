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
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include <string.h>
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

/*
 * 256 colours (a TT or a Falcon): upstream's ramps, from its own
 * formulas, at the bases koules.h names. The grey runs bright to dark
 * like the others, where upstream's ran the other way for its stars;
 * entry 0 keeps the playfield blue the sixteen have.
 */
static void
palette256 (Palette * pal)
{
  int             i;

  memset (pal, 0, sizeof (*pal));
  for (i = 0; i < 32; i++)
    {
      int             k = i + 1, v = (32 - i) << 1;
      int             w = (8 - k) > 0 ? 8 - k : 0;

      pal->color[32 + i].blue = (unsigned char) (i << 1);
      pal->color[64 + i].red = (unsigned char) v;
      pal->color[64 + i].green = pal->color[64 + i].blue =
	(unsigned char) ((w << 5) / 5);
      pal->color[96 + i].red = pal->color[96 + i].green = (unsigned char) v;
      pal->color[96 + i].blue = (unsigned char) (w << 3);
      pal->color[128 + i].green = (unsigned char) v;
      pal->color[128 + i].red = pal->color[128 + i].blue =
	(unsigned char) (w << 3);
      pal->color[160 + i].blue = (unsigned char) v;
      pal->color[160 + i].red = pal->color[160 + i].green =
	(unsigned char) ((w << 3) / 2);
      pal->color[192 + i].red = pal->color[192 + i].green =
	pal->color[192 + i].blue = (unsigned char) (v > 63 ? 63 : v);
    }
  pal->color[0].blue = 18;
  pal->color[255].red = pal->color[255].green = pal->color[255].blue = 63;
}

void
setcustompalette (CONST int p, CONST float p1)
{
  Palette         pal;
  int             i;

  if (k256)
    palette256 (&pal);
  else
    for (i = 0; i < 16; i++)
      {
	pal.color[i].red = basepal[i][0];
	pal.color[i].green = basepal[i][1];
	pal.color[i].blue = basepal[i][2];
      }
  for (i = 0; i < NCOLOURS; i++)
    {
      pal.color[i].red = col (pal.color[i].red + p, p1);
      pal.color[i].green = col (pal.color[i].green + p, p1);
      pal.color[i].blue = col (pal.color[i].blue + p, p1);
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
