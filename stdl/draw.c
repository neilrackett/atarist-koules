/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  Atari ST / STDL backend                                 *
*----------------------------------------------------------*
*  stdl/draw.c  drawing routines on top of STDL            *
***********************************************************/

#include <interface.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../koules.h"
#include "font8x8.h"

VScreenType     backscreen;     /* == the live screen                 */
VScreenType     background;     /* what RestoreBackground repaints    */
VScreenType     starbackground; /* unused on the ST (see stdl/intro.c) */

VScreenType     current;

/* SDL_gfx's 8x8 cell font, the same one the SDL backend draws with,
 * wrapped in an STDL_Font so glyphs come out pixel-identical. */
static STDL_Font kfont = { 8, 8, 0, 255, 1, (uint8_t *) font_data };

/*
 * Dirty rectangles.  Every draw that lands on the screen records its
 * bounding box; RestoreBackground() repaints those boxes from the
 * background surface at the top of the next frame.  Overflowing the
 * list is not an error - STDL_Dirty falls back to a full-screen
 * restore, which is merely slow.
 */
#define MAXDIRTY 400

/*
 * Menus and briefings are overlays, not moving objects: they change
 * only when the player changes them.  While one is up its draws are
 * not recorded, so the next frame's restore leaves it alone and it
 * is repainted only when the game says it has changed - the
 * difference between drawing four hundred glyphs every frame and
 * drawing them once.
 */
static int      nodirty;

void
SuppressDirty (int on)
{
  nodirty = on;
}

/* Wipe the playfield (not the status bar) ready for a new overlay. */
void
ClearOverlay (void)
{
  STDL_Rect       r;
  r.x = 0;
  r.y = 0;
  r.w = MAPWIDTH;
  r.h = MAPHEIGHT;
  STDL_FillRect (backscreen, &r, C_BG);
}

static void
dirty (int x, int y, int w, int h)
{
  STDL_Rect       r;
  if (current != backscreen || nodirty)
    return;
  if (x < 0)
    w += x, x = 0;
  if (y < 0)
    h += y, y = 0;
  if (x >= MAPWIDTH || y >= MAPHEIGHT + 20 || w <= 0 || h <= 0)
    return;
  if (x + w > MAPWIDTH)
    w = MAPWIDTH - x;
  if (y + h > MAPHEIGHT + 20)
    h = MAPHEIGHT + 20 - y;
  r.x = x;
  r.y = y;
  r.w = w;
  r.h = h;
  STDL_DirtyPush (&r);
}

void
DirtyAll (void)
{
  STDL_Rect       r;
  r.x = 0;
  r.y = 0;
  r.w = MAPWIDTH;
  r.h = MAPHEIGHT + 20;
  STDL_DirtyPush (&r);
}

void
RestoreBackground (void)
{
  STDL_DirtyRestore (backscreen);
}

/*
 * Particles.  MAXPOINT of them, one pixel each: a dirty rectangle
 * apiece would cost more than the pixel does, so they are buffered
 * and drawn with one batched STDL_Points call per colour, then
 * erased next frame with a single call in the playfield colour.
 * points() keeps every particle inside y < MAPHEIGHT, where the
 * background is flat, so filling is an exact erase.
 */
static STDL_Point ptin[MAXPOINT];
static uint8_t  ptcol[MAXPOINT];
static int      nptin;
static STDL_Point ptout[MAXPOINT];
static int      nptout;
static int      ptstart[17];

void
SMySetPixel (VScreenType screen, int x, int y, int c)
{
  (void) screen;
  if (nptin < MAXPOINT)
    {
      ptin[nptin].x = x;
      ptin[nptin].y = y >> 8;
      ptcol[nptin] = c;
      nptin++;
    }
}

/*
 * Erasing the particle field.  Below the threshold, writing the
 * background colour back over each pixel is exact and cheap.  Above
 * it, one background blit over the particles' bounding box is
 * strictly less work: a single-pixel span still pays a clip test, a
 * row-address multiply and a read-modify-write per plane (measured
 * at roughly 550 cycles), while the blit moves whole groups.  At a
 * few hundred particles - one rocket explosion is about 150 - the
 * blit wins, and it costs nothing extra because the dirty restore
 * that follows has to run anyway.
 */
#define PT_BULK_ERASE 160       /* below this, never worth a bbox scan  */
#define PT_BULK_RATIO  92       /* pixels a blit does per span's cycles  */

void
ErasePoints (void)
{
  if (nptout == 0)
    return;
  if (nptout >= PT_BULK_ERASE)
    {
      int             i, x0 = MAPWIDTH, y0 = MAPHEIGHT, x1 = -1, y1 = -1;
      STDL_Rect       r;

      for (i = 0; i < nptout; i++)
	{
	  int             x = ptout[i].x, y = ptout[i].y;
	  if (x < x0) x0 = x;
	  if (x > x1) x1 = x;
	  if (y < y0) y0 = y;
	  if (y > y1) y1 = y;
	}
      r.x = x0;
      r.y = y0;
      r.w = x1 - x0 + 1;
      r.h = y1 - y0 + 1;
      /* measured on a plain ST: a background blit runs at about six
         cycles a pixel, a one-pixel span at about 550, so the blit
         wins while the box holds fewer than ~92 pixels per particle.
         A fresh explosion is tight and takes this path; the same
         particles a second later cover the screen and do not. */
      if ((int32_t) r.w * r.h < (int32_t) nptout * PT_BULK_RATIO)
	{
	  STDL_DirtyPush (&r);
	  nptout = 0;
	  return;
	}
    }
  STDL_Points (backscreen, ptout, nptout, C_BG);
  nptout = 0;
}

void
FlushPoints (void)
{
  int             cnt[16];
  int             i, c, n;

  n = nptin;
  nptin = 0;
  if (n == 0)
    return;

  /* counting sort into colour runs: two linear passes, then at most
     sixteen batched span calls instead of n single-pixel ones */
  for (c = 0; c < 16; c++)
    cnt[c] = 0;
  for (i = 0; i < n; i++)
    cnt[ptcol[i]]++;
  ptstart[0] = 0;
  for (c = 0; c < 16; c++)
    ptstart[c + 1] = ptstart[c] + cnt[c];
  for (c = 0; c < 16; c++)
    cnt[c] = ptstart[c];
  for (i = 0; i < n; i++)
    ptout[cnt[ptcol[i]]++] = ptin[i];

  for (c = 1; c < 16; c++)      /* colour 0 is the background */
    {
      int             k = ptstart[c + 1] - ptstart[c];
      if (k)
	STDL_Points (backscreen, ptout + ptstart[c], k, c);
    }
  nptout = n;
}

/* ---------------------------------------------------------------- */
/* sprites                                                          */

/* Allocate a surface for a sprite (ball) under construction. */
RawBitmapType
CreateBitmap (const int xv, const int yv)
{
  KBitmap        *b = (KBitmap *) calloc (1, sizeof (KBitmap));

  if (b == NULL)
    return NULL;
  b->w = xv;
  b->h = yv;
  b->surf = STDL_CreateSurface (xv, yv);
  if (b->surf == NULL)
    {
      free (b);
      return NULL;
    }
  return b;
}

/*
 * Freeze the surface into a pre-shifted sprite.  Colour 0 is the key:
 * CreateBitmap zeroes the surface and BSetPixel drops colour 0
 * writes, so everything outside the ball is already transparent.
 */
BitmapType
CompileBitmap (const int x, const int y, const RawBitmapType b)
{
  (void) x;
  (void) y;
  if (b == NULL || b->surf == NULL)
    return b;
  STDL_SetColourKey (b->surf, 1, C_KEY);
  b->spr = STDL_SpriteFromSurface (b->surf, b->w, SPRITE_FLAGS);
  if (b->spr == NULL && SPRITE_FLAGS != 0)
    {
      /* the heap check in stdl/init.c should have caught this, but a
         sprite that draws slowly beats one that does not draw */
      b->spr = STDL_SpriteFromSurface (b->surf, b->w, 0);
    }
  STDL_FreeSurface (b->surf);
  b->surf = NULL;
  if (b->spr == NULL)
    fprintf (stderr, "sprite: %s\n", STDL_GetError ());
  return b;
}

/* Set a pixel of a ball sprite. c == 0 is transparent. Load time
 * only - this is exactly the sanctioned use of a per-pixel write. */
void
BSetPixel (RawBitmapType bitmap, int x, int y, int c)
{
  if (c && bitmap != NULL && bitmap->surf != NULL)
    STDL_PutPixel (bitmap->surf, x, y, c);
}

/* Draw a sprite onto the current surface. The size arguments are
 * upstream's idea of the sprite size and are not always right (the
 * hole passes HOLE_RADIUS*2); the sprite knows its own. */
void
PutBitmap (const int x, const int y, const int xsize, const int ysize,
	   const BitmapType bitmap)
{
  (void) xsize;
  (void) ysize;
  if (bitmap == NULL || bitmap->spr == NULL)
    return;
  STDL_BlitSprite (bitmap->spr, 0, current, x, y);
  dirty (x, y, bitmap->w, bitmap->h);
}

/* ---------------------------------------------------------------- */
/* screens                                                          */

void
SetScreen (VScreenType screen)
{
  current = screen;
}

static int      statusvalid;

void
ClearScreen (void)
{
  STDL_FillRect (current, NULL, C_BG);
  if (current == backscreen)
    {
      /* Everything that was on screen has gone, so the next restore
         has to repaint the lot rather than the boxes drawn into the
         frame we just threw away. */
      nptout = 0;
      statusvalid = 0;
      STDL_DirtyReset ();
      DirtyAll ();
    }
}

/* Nothing to do: the game draws straight into screen memory. */
void
CopyToScreen (VScreenType source)
{
  (void) source;
}

void
CopyVSToVS (VScreenType source, VScreenType destination)
{
  if (source == NULL || destination == NULL)
    return;
  STDL_BlitSurface (source, NULL, destination, NULL);
}

/* STDL always clips; these stay so the call sites read unchanged. */
void
EnableClipping (void)
{
}

void
DisableClipping (void)
{
}

/* ---------------------------------------------------------------- */
/* primitives                                                       */

int
SGetPixel (int x, int y)
{
  return STDL_GetPixel (current, x, y);
}

void
SPutPixel (int x, int y, int c)
{
  STDL_PutPixel (current, x, y, c);
  dirty (x, y, 1, 1);
}

void
SSetPixel (int x, int y, int c)
{
  STDL_PutPixel (current, x, y, c);
  dirty (x, y, 1, 1);
}

void
Line (int x1, int y1, int x2, int y2, int c)
{
  STDL_Line (current, x1, y1, x2, y2, c);
  if (current == backscreen)
    {
      int             lx = x1 < x2 ? x1 : x2;
      int             ly = y1 < y2 ? y1 : y2;
      dirty (lx, ly, abs (x2 - x1) + 1, abs (y2 - y1) + 1);
    }
}

/* Perspective line, starwars scroller only - kept so the interface
 * is complete; stdl/intro.c does not use it. */
void
Line1 (int x1, int y1, int x2, int y2, int c)
{
  x1 = (MAPWIDTH / 2 + (x1) * 220 / (1000 - (y1)) / DIV);
  y1 = (MAPHEIGHT / 3 + MAPWIDTH * 220 / (1000 - (y1)));
  x2 = (MAPWIDTH / 2 + (x2) * 220 / (1000 - (y2)) / DIV);
  y2 = (MAPHEIGHT / 3 + MAPWIDTH * 220 / (1000 - (y2)));
  Line (x1, y1, x2, y2, c);
}

void
HLine (int x1, int y1, int x2, int c)
{
  Line1 (x1, y1, x2, y1, c);
}

/* Menu selection frame: four edges, so the dirty rectangles are four
 * one-pixel strips rather than the whole enclosed area. */
void
DrawRectangle (int x1, int y1, int x2, int y2, int color)
{
  STDL_HLine (current, x1, x2, y1, color);
  STDL_HLine (current, x1, x2, y2, color);
  STDL_VLine (current, x1, y1, y2, color);
  STDL_VLine (current, x2, y1, y2, color);
  if (current == backscreen)
    {
      int             w = x2 - x1 + 1;
      int             h = y2 - y1 + 1;
      dirty (x1, y1, w, 1);
      dirty (x1, y2, w, 1);
      dirty (x1, y1, 1, h);
      dirty (x2, y1, 1, h);
    }
}

static void
text (int x, int y, const char *s, int col)
{
  STDL_DrawText (current, &kfont, x, y, s, col);
  dirty (x, y, (int) strlen (s) * 8, 8);
}

void
DrawText (int x, int y, char *s)
{
  text (x, y, s, C_WHITE);
}

/* Upstream's shadow pass. Black is the playfield colour here, so the
 * shadow disappears into the background and outlines the white text
 * only where it crosses a sprite - which is what it is for. */
void
DrawBlackMaskedText (int x, int y, char *s)
{
  text (x, y, s, C_BG);
}

void
DrawWhiteMaskedText (int x, int y, char *s)
{
  text (x, y, s, C_WHITE);
}

/*
 * The 20-row status bar under the playfield.  Nothing else draws
 * there and no particle can reach it, so it survives untouched from
 * frame to frame: repaint only when the strings actually change.
 */
void
StatusBar (const char *lives, const char *scores)
{
  static char     lastlives[64];
  static char     lastscores[64];
  STDL_Rect       r;
  int             changed = !statusvalid;

  /* one line at a time: the score line carries the frame counter and
     so changes once a second, the lives line hardly ever */
  if (changed || strncmp (lives, lastlives, sizeof (lastlives) - 1))
    {
      strncpy (lastlives, lives, sizeof (lastlives) - 1);
      r.x = 0;
      r.y = MAPHEIGHT + 1;
      r.w = MAPWIDTH;
      r.h = 9;
      STDL_FillRect (backscreen, &r, C_BG);
      STDL_DrawText (backscreen, &kfont,
		     MAPWIDTH / 2 - (int) strlen (lives) * 4,
		     MAPHEIGHT + 2, lives, C_WHITE);
    }
  if (changed || strncmp (scores, lastscores, sizeof (lastscores) - 1))
    {
      strncpy (lastscores, scores, sizeof (lastscores) - 1);
      r.x = 0;
      r.y = MAPHEIGHT + 10;
      r.w = MAPWIDTH;
      r.h = 10;
      STDL_FillRect (backscreen, &r, C_BG);
      STDL_DrawText (backscreen, &kfont,
		     MAPWIDTH / 2 - (int) strlen (scores) * 4,
		     MAPHEIGHT + 11, scores, C_WHITE);
    }
  statusvalid = 1;
}

/*
 * Full-screen text page: the level briefings upstream scrolls past in
 * perspective with a vector font (see the report for what was cut).
 */
void
TextPage (const char *const *lines, int nlines)
{
  int             i, y;
  VScreenType     save = current;

  current = backscreen;
  STDL_FillRect (backscreen, NULL, C_BG);
  y = (MAPHEIGHT + 20 - nlines * 10) / 2;
  if (y < 2)
    y = 2;
  for (i = 0; i < nlines && y < MAPHEIGHT + 12; i++, y += 10)
    {
      int             len = (int) strlen (lines[i]);
      if (len > 40)
	len = 40;
      STDL_DrawText (backscreen, &kfont, MAPWIDTH / 2 - len * 4, y,
		     lines[i], C_WHITE);
    }
  current = save;
  DirtyAll ();
  nptout = 0;
}

/* ---------------------------------------------------------------- */
/* palette                                                          */

void
WaitRetrace (void)
{
}

void
SetPalette (Palette * pal)
{
  STDL_Colour     c[COLORS];
  int             i;

  for (i = 0; i < COLORS; i++)
    {
      /* upstream keeps 0..63 VGA components; the ST wants 0..255 */
      c[i].r = pal->color[i].red << 2;
      c[i].g = pal->color[i].green << 2;
      c[i].b = pal->color[i].blue << 2;
      c[i].unused = 0;
    }
  STDL_SetColours (backscreen, c, 0, COLORS);
}
