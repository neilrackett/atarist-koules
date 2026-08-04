/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  Atari ST / STDL backend                                 *
*----------------------------------------------------------*
*  stdl/draw.c  drawing routines on top of STDL            *
***********************************************************/

/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/
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
 * only when the player changes them, so painting one every frame is
 * four hundred glyphs for nothing.  They are painted into the
 * background surface instead - the very surface RestoreBackground
 * repaints from - and blitted to the screen once.  While a menu is
 * up the background *is* the menu, which is what lets the selection
 * frame slide over it: DrawSelector() puts the four strips it last
 * covered back from there, no repaint involved.
 */
static int      overlaybg;      /* background holds a menu, not the map */
static int      selvalid;       /* a selection frame is on screen       */

static void
playfield (STDL_Rect * r)
{
  r->x = 0;
  r->y = 0;
  r->w = MAPWIDTH;
  r->h = MAPHEIGHT;
}

/* Start painting an overlay: draws go to the background surface, so
   nothing is recorded dirty and nothing appears on screen yet. */
void
OverlayBegin (void)
{
  STDL_Rect       r;
  playfield (&r);
  STDL_FillRect (background, &r, C_BG);
  current = background;
}

/* Finished painting: put the whole playfield on screen in one go.
   Full width and word aligned, so this is the blit STDL is fastest
   at - and it happens when the menu changes, not when it moves. */
void
OverlayEnd (void)
{
  STDL_Rect       src, dst;
  current = backscreen;
  overlaybg = 1;
  selvalid = 0;                 /* the blit takes the frame with it */
  playfield (&src);
  dst = src;
  STDL_BlitSurface (background, &src, backscreen, &dst);
}

/* The background surface goes back to being the plain playfield.
   Callers that are not already repainting the screen have to take
   the overlay off it as well - see OverlayDrop. */
static void
drop_overlay (void)
{
  STDL_Rect       r;
  if (!overlaybg)
    return;
  overlaybg = 0;
  selvalid = 0;
  playfield (&r);
  STDL_FillRect (background, &r, C_BG);
}

/* The overlay has gone: forget it and repaint over it. */
void
OverlayDrop (void)
{
  STDL_Rect       r;
  if (!overlaybg)
    return;
  drop_overlay ();
  playfield (&r);
  STDL_DirtyPush (&r);
}

static void
dirty (int x, int y, int w, int h)
{
  STDL_Rect       r;
  if (current != backscreen)
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
 * and drawn with one batched STDL_PointsC call, then erased next
 * frame with a single STDL_Points call in the playfield colour.
 * points() keeps every particle inside y < MAPHEIGHT, where the
 * background is flat, so filling is an exact erase.
 *
 * One buffer serves both passes because the frame erases before it
 * steps: ErasePoints() reads the list FlushPoints() left there last
 * frame, and only then does points() overwrite it.
 */
STDL_Point      kpt_xy[KPT_MAX];
uint8_t         kpt_col[KPT_MAX];
int             kpt_n;
static int      nptout;         /* of kpt_xy, still on screen        */

#if KPT_MAX < MAXPOINT
#error "KPT_MAX must cover MAXPOINT"
#endif

/*
 * Erasing the particle field.  Below the threshold, writing the
 * background colour back over each pixel is exact and cheap.  Above
 * it, one background blit over the particles' bounding box can be
 * less work: a single point still pays a clip test, a row-address
 * multiply and a read-modify-write per plane pair (measured at 288
 * cycles), while the blit moves whole groups at about six cycles a
 * pixel.  That is the whole heuristic - and note that making the
 * point path faster moves the crossover, so PT_BULK_RATIO is
 * 288/6, not a constant of nature.
 */
#define PT_BULK_ERASE 160       /* below this, never worth a bbox scan  */
#define PT_BULK_RATIO  48       /* pixels a blit does per point's cycles */

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
	  int             x = kpt_xy[i].x, y = kpt_xy[i].y;
	  if (x < x0) x0 = x;
	  if (x > x1) x1 = x;
	  if (y < y0) y0 = y;
	  if (y > y1) y1 = y;
	}
      r.x = x0;
      r.y = y0;
      r.w = x1 - x0 + 1;
      r.h = y1 - y0 + 1;
      /* a fresh explosion is tight and takes this path; the same
         particles a second later cover the screen and do not */
      if ((int32_t) r.w * r.h < (int32_t) nptout * PT_BULK_RATIO)
	{
	  STDL_DirtyPush (&r);
	  nptout = 0;
	  return;
	}
    }
  STDL_Points (backscreen, kpt_xy, nptout, C_BG);
  nptout = 0;
}

/*
 * Upstream's particle field is multi-coloured, so this used to
 * counting-sort the list into colour runs and make a batched call
 * per run - the only way to use a primitive that takes one colour.
 * On a plain ST at 250 particles the sort and the fifteen calls cost
 * 23ms against 13.5 for one STDL_PointsC, so the colour now travels
 * with the point and the sort is gone.
 */
void
FlushPoints (void)
{
  int             n = kpt_n;

  kpt_n = 0;
  if (n == 0)
    return;
  STDL_PointsC (backscreen, kpt_xy, kpt_col, n);
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
         frame we just threw away - and any overlay went with it. */
      nptout = 0;
      statusvalid = 0;
      drop_overlay ();
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

/*
 * The menu selection frame: two nested rectangles, the only thing
 * that moves while a menu is up.
 *
 * It does not go through the dirty list.  The screen is the live
 * framebuffer, and the restore that would erase it runs at the top
 * of the frame while the redraw runs at the bottom, so for the
 * milliseconds in between there is no frame on screen at all - at
 * 50Hz that is a visible blink several times a second.  Erasing it
 * from the background surface immediately before redrawing it closes
 * that window, and a frame that has not moved is left alone
 * completely, so a settled menu is perfectly still.
 */
static STDL_Rect selrect;       /* what is on screen, 2px edges       */

static void
unselect (void)
{
  STDL_Rect       s, d;
  int             i;

  if (!selvalid)
    return;
  for (i = 0; i < 4; i++)
    {
      s = selrect;
      switch (i)
	{
	case 0:
	  s.h = 2;
	  break;                /* top    */
	case 1:
	  s.y += s.h - 2, s.h = 2;
	  break;                /* bottom */
	case 2:
	  s.w = 2;
	  break;                /* left   */
	case 3:
	  s.x += s.w - 2, s.w = 2;
	  break;                /* right  */
	}
      d = s;
      STDL_BlitSurface (background, &s, backscreen, &d);
    }
  selvalid = 0;
}

void
DrawSelector (int x1, int y1, int x2, int y2, int col1, int col2)
{
  STDL_Rect       r;

  r.x = (int16_t) x1;
  r.y = (int16_t) y1;
  r.w = (uint16_t) (x2 - x1 + 2); /* +1 for the second, offset frame */
  r.h = (uint16_t) (y2 - y1 + 2);
  if (selvalid && r.x == selrect.x && r.y == selrect.y
      && r.w == selrect.w && r.h == selrect.h)
    return;                     /* still where we left it */
  unselect ();
  selrect = r;
  selvalid = 1;
  STDL_HLine (backscreen, x1, x2, y1, col1);
  STDL_HLine (backscreen, x1, x2, y2, col1);
  STDL_VLine (backscreen, x1, y1, y2, col1);
  STDL_VLine (backscreen, x2, y1, y2, col1);
  STDL_HLine (backscreen, x1 + 1, x2 + 1, y1 + 1, col2);
  STDL_HLine (backscreen, x1 + 1, x2 + 1, y2 + 1, col2);
  STDL_VLine (backscreen, x1 + 1, y1 + 1, y2 + 1, col2);
  STDL_VLine (backscreen, x2 + 1, y1 + 1, y2 + 1, col2);
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

/* Same, in a colour of the caller's choosing. */
void
DrawColorText (int x, int y, char *s, int color)
{
  text (x, y, s, color);
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
  drop_overlay ();              /* whatever was up has been painted over */
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
