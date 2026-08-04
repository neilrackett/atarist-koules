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

VScreenType     backscreen;     /* the page being drawn into          */
VScreenType     background;     /* what RestoreBackground repaints    */
VScreenType     starbackground; /* unused on the ST (see stdl/intro.c) */

VScreenType     current;

/* SDL_gfx's 8x8 cell font, the same one the SDL backend draws with,
 * wrapped in an STDL_Font so glyphs come out pixel-identical. */
static STDL_Font kfont = { 8, 8, 0, 255, 1, (uint8_t *) font_data };

/*
 * Page flipping.  Drawing a frame means erasing what moved and then
 * putting it back somewhere else, which takes about as long as one
 * raster sweep: straight onto the visible screen, every object the
 * beam passes between its erase and its redraw is missing from that
 * sweep, and a screen full of koules flickers.  So STDL is asked for
 * two pages when there is memory for them (stdl/init.c decides) and
 * the frame is drawn into the one nobody is looking at, becoming
 * visible in a single VBL-synced flip.
 *
 * backscreen is STDL's screen surface, whose pixel base STDL_Flip
 * swaps: it stays the same pointer, so `current == backscreen` still
 * means "drawing on the screen" everywhere below.
 */
#define DOUBLED  ((backscreen->flags & STDL_DOUBLEBUF) != 0)

static int      dpage;          /* the page being drawn into */

/*
 * Dirty rectangles, one list per page.
 *
 * Every draw that lands on a page records its bounding box, and the
 * top of the next frame repaints those boxes from the background
 * surface.  With two pages the one about to be drawn into is two
 * frames old rather than one, so what has to be put back is a
 * property of the page, not of the frame: each page carries the boxes
 * that were last drawn into *it*, and the other page's boxes are none
 * of its business.  That keeps the restore exactly as big as it was
 * single-buffered - a union of the last two frames would repaint
 * twice the area for nothing.
 *
 * Overflowing a list is not an error: it turns into a whole-page
 * restore, which is merely slow.
 */
#define MAXDIRTY 400
static STDL_Rect drect[2][MAXDIRTY];
static int      dn[2];
static int      dfull[2];       /* repaint the whole page */

static void
push (int p, const STDL_Rect * r)
{
  if (dn[p] >= MAXDIRTY)
    dfull[p] = 1;
  else
    drect[p][dn[p]++] = *r;
}

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
static int      selvalid[2];    /* a selection frame is on this page    */

/* The status bar is repainted only when it changes, so with two pages
   a change owes a repaint to each of them; see StatusBar. */
static int      pendlives, pendscores;

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
   at - and it happens when the menu changes, not when it moves.
   The other page gets the same picture from its own restore: while a
   menu is up the background surface *is* the menu, so a playfield
   rectangle on its dirty list says exactly the same thing. */
void
OverlayEnd (void)
{
  STDL_Rect       src, dst;
  current = backscreen;
  overlaybg = 1;
  selvalid[0] = selvalid[1] = 0;  /* the blit takes the frame with it */
  playfield (&src);
  dst = src;
  STDL_BlitSurface (background, &src, backscreen, &dst);
  if (DOUBLED)
    push (dpage ^ 1, &src);
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
  selvalid[0] = selvalid[1] = 0;
  playfield (&r);
  STDL_FillRect (background, &r, C_BG);
}

/* The overlay has gone: forget it and repaint over it. */
void
OverlayDrop (void)
{
  if (!overlaybg)
    return;
  drop_overlay ();
  DirtyAll ();
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
  push (dpage, &r);
}

/* Both pages have to be repainted in full. */
void
DirtyAll (void)
{
  dn[0] = dn[1] = 0;
  dfull[0] = dfull[1] = 1;
}

void
RestoreBackground (void)
{
  int             i;

  if (dfull[dpage])
    {
      /* The background surface has no status text in it and no
         selection frame, so a whole-page repaint owes this page
         both back.  Set here rather than where the full restore is
         asked for, because an overflowing list asks for one too. */
      STDL_BlitSurface (background, NULL, backscreen, NULL);
      selvalid[dpage] = 0;
      if (pendlives < 1)
	pendlives = 1;
      if (pendscores < 1)
	pendscores = 1;
    }
  else
    for (i = 0; i < dn[dpage]; i++)
      {
	STDL_Rect       d = drect[dpage][i];
	STDL_BlitSurface (background, &drect[dpage][i], backscreen, &d);
      }
  dn[dpage] = 0;
  dfull[dpage] = 0;
}

/*
 * Particles.  MAXPOINT of them, one pixel each: a dirty rectangle
 * apiece would cost more than the pixel does, so they are buffered
 * and drawn with one batched STDL_PointsC call, then erased next
 * frame with a single STDL_Points call in the playfield colour.
 * points() keeps every particle inside y < MAPHEIGHT, where the
 * background is flat, so filling is an exact erase.
 *
 * One buffer per page serves both passes because the frame erases
 * before it steps: ErasePoints() reads the list FlushPoints() left
 * there when this page was last drawn, and only then does points()
 * overwrite it.  Two pages need two lists for the same reason the
 * dirty rectangles do - the particles still on a page are the ones
 * put there two frames ago.  Colours are not doubled: they are only
 * read by the draw pass, which rebuilds them every frame.
 */
static STDL_Point kpt_buf[2][KPT_MAX];
STDL_Point     *kpt_xy = kpt_buf[0];
uint8_t         kpt_col[KPT_MAX];
int             kpt_n;
static int      nptout[2];      /* of kpt_buf[p], still on that page */

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
  STDL_Point     *out = kpt_buf[dpage];
  int             n = nptout[dpage];

  if (n == 0)
    return;
  if (n >= PT_BULK_ERASE)
    {
      int             i, x0 = MAPWIDTH, y0 = MAPHEIGHT, x1 = -1, y1 = -1;
      STDL_Rect       r;

      for (i = 0; i < n; i++)
	{
	  int             x = out[i].x, y = out[i].y;
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
      if ((int32_t) r.w * r.h < (int32_t) n * PT_BULK_RATIO)
	{
	  push (dpage, &r);
	  nptout[dpage] = 0;
	  return;
	}
    }
  STDL_Points (backscreen, out, n, C_BG);
  nptout[dpage] = 0;
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
  nptout[dpage] = n;
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

void
ClearScreen (void)
{
  STDL_FillRect (current, NULL, C_BG);
  if (current == backscreen)
    {
      /* Everything that was on screen has gone, so the next restore
         has to repaint the lot rather than the boxes drawn into the
         frame we just threw away - and any overlay went with it.
         The page we did not clear is repainted from the background
         instead, which comes to the same thing. */
      nptout[0] = nptout[1] = 0;
      drop_overlay ();
      DirtyAll ();
    }
}

/*
 * Show the frame that has just been drawn.  Double-buffered this is
 * the page flip, which is where the whole cooperative frame waits for
 * the raster; single-buffered the game drew straight into screen
 * memory and there is nothing to do.
 *
 * Called once per *drawn* frame - draw_objects(0) skips it - so a
 * skipped frame leaves the last complete picture on screen instead of
 * showing a page nothing was drawn into.
 */
void
CopyToScreen (VScreenType source)
{
  (void) source;
  if (!DOUBLED)
    return;
  STDL_Flip ();
  dpage ^= 1;
  kpt_xy = kpt_buf[dpage];
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
 *
 * Where it is, is per page: a frame that has settled still has to be
 * drawn once into each of them before either can be left alone.
 */
static STDL_Rect selrect[2];    /* what is on the page, 2px edges     */

static void
unselect (void)
{
  STDL_Rect       s, d;
  int             i;

  if (!selvalid[dpage])
    return;
  for (i = 0; i < 4; i++)
    {
      s = selrect[dpage];
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
  selvalid[dpage] = 0;
}

void
DrawSelector (int x1, int y1, int x2, int y2, int col1, int col2)
{
  STDL_Rect       r;

  r.x = (int16_t) x1;
  r.y = (int16_t) y1;
  r.w = (uint16_t) (x2 - x1 + 2); /* +1 for the second, offset frame */
  r.h = (uint16_t) (y2 - y1 + 2);
  if (selvalid[dpage] && r.x == selrect[dpage].x && r.y == selrect[dpage].y
      && r.w == selrect[dpage].w && r.h == selrect[dpage].h)
    return;                     /* still where we left it */
  unselect ();
  selrect[dpage] = r;
  selvalid[dpage] = 1;
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
 * frame to frame: repaint only when the strings actually change - and
 * then once per page, because a page that was not on screen when they
 * changed still carries the old numbers.  Called every frame; the two
 * strncmps that a settled bar costs are nothing beside the eight
 * hundred pixels of a repaint.
 */
void
StatusBar (const char *lives, const char *scores)
{
  static char     lastlives[64];
  static char     lastscores[64];
  STDL_Rect       r;
  int             npages = DOUBLED ? 2 : 1;

  /* one line at a time: the score line carries the frame counter and
     so changes once a second, the lives line hardly ever */
  if (strncmp (lives, lastlives, sizeof (lastlives) - 1))
    {
      strncpy (lastlives, lives, sizeof (lastlives) - 1);
      pendlives = npages;
    }
  if (pendlives)
    {
      pendlives--;
      r.x = 0;
      r.y = MAPHEIGHT + 1;
      r.w = MAPWIDTH;
      r.h = 9;
      STDL_FillRect (backscreen, &r, C_BG);
      STDL_DrawText (backscreen, &kfont,
		     MAPWIDTH / 2 - (int) strlen (lastlives) * 4,
		     MAPHEIGHT + 2, lastlives, C_WHITE);
    }
  if (strncmp (scores, lastscores, sizeof (lastscores) - 1))
    {
      strncpy (lastscores, scores, sizeof (lastscores) - 1);
      pendscores = npages;
    }
  if (pendscores)
    {
      pendscores--;
      r.x = 0;
      r.y = MAPHEIGHT + 10;
      r.w = MAPWIDTH;
      r.h = 10;
      STDL_FillRect (backscreen, &r, C_BG);
      STDL_DrawText (backscreen, &kfont,
		     MAPWIDTH / 2 - (int) strlen (lastscores) * 4,
		     MAPHEIGHT + 11, lastscores, C_WHITE);
    }
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
  nptout[0] = nptout[1] = 0;
}

/* ---------------------------------------------------------------- */
/* palette                                                          */

/*
 * Upstream calls this before every palette write, and it means what
 * it says: the shifter latches colour registers as it scans, so a
 * fade step landing mid-frame splits the picture across two of its
 * eleven shades.  The page flip is not here - it belongs with the
 * frame, in CopyToScreen - because the fades program the palette a
 * dozen times in a row and flipping pages under them would show a
 * frame nothing had been drawn into.
 */
void
WaitRetrace (void)
{
  STDL_WaitVBL ();
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
