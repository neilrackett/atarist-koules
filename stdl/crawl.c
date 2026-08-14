/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  Atari ST / STDL backend                                 *
*----------------------------------------------------------*
*  stdl/crawl.c  the starwars scroller, planar edition     *
*----------------------------------------------------------*
* Upstream draws the crawl with font.c: a vector font       *
* whose every stroke is projected and clipped per frame -   *
* per-pixel work behind a 65fps target, which is why the    *
* first ST port replaced it with static pages.  This file   *
* brings the crawl back by changing the shape of the work:  *
*                                                           *
* A text line is a single-colour billboard at one depth,    *
* and its depth changes slowly.  So each line is rendered   *
* ONCE from the 8x8 system font, nearest-neighbour scaled   *
* to its current pixel height, expanded to a planar         *
* surface, and cached; it is re-rendered only when its      *
* height bucket changes (a few times a second across the    *
* whole screen), and only remapped when just its fade       *
* shade changes.  The per-frame cost is a handful of        *
* 16px-aligned colour-keyed blits plus the dirty-box        *
* restores - exactly the work an 8MHz 68000 is good at.     *
*                                                           *
* The projection is upstream's:                             *
*    screen_y = MAPHEIGHT/3 + MAPWIDTH*220 / (1000 - d)     *
* with d the line's depth (line*TEXTW - i), and the fade    *
* is upstream's (1200+d)*32/2500 collapsed onto the four    *
* usable shades of the sixteen-colour palette.              *
*                                                           *
* Downscaling an 8x8 glyph below 8px with plain nearest     *
* neighbour drops columns, and a dropped column can be the  *
* stem of an 'i'.  Sampling therefore ORs the source range  *
* covered by each output pixel - letters get slightly       *
* bolder as they shrink instead of falling apart.           *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include <interface.h>
#include <string.h>

#include "../koules.h"
#include "../physics.h"

#ifdef KOULES_FLOAT
#include <math.h>
#endif

#define TEXTW    200            /* depth between lines, upstream's   */
#define D_NEAR   600            /* depth culling, upstream's numbers */
#define D_FAR    (-1200)
#define HORIZON  (MAPHEIGHT / 3)
#define PROJ     (MAPWIDTH * 220L)
#define GLYPROJ  8000L          /* glyph height h = GLYPROJ/(1000-d) */
#define HMIN     3
#define HMAX     16
#define NSTARS   100

extern void     points (void);  /* particle step + draw, koules.c */

/* sin/cos in 16.16 for whole degrees; the fixed build has the table,
 * the SFP build has (and links) the real thing */
#ifdef KOULES_FLOAT
#define DEG2RAD 0.017453292519943295
static long
ksin (int deg)
{
  return (long) (sin (deg * DEG2RAD) * 65536.0);
}
static long
kcos (int deg)
{
  return (long) (cos (deg * DEG2RAD) * 65536.0);
}
#else
#define ksin(deg) ((long) fixsin (deg))
#define kcos(deg) ((long) fixcos (deg))
#endif

/* ---------------------------------------------------------------- */
/* glyph scaling                                                    */

/*
 * exptab[h][byte] is a font row byte stretched to h pixels, MSB
 * aligned in a 16-bit word, with OR-sampling.  Built lazily, one
 * height at a time; 256 entries per height is a load-time loop, not
 * a rendering one.
 */
static uint16_t exptab[HMAX + 1][256];
static uint8_t  expdone[HMAX + 1];

static void
ensure_exptab (int h)
{
  uint8_t         m[HMAX];
  int             ox, b;

  if (expdone[h])
    return;
  for (ox = 0; ox < h; ox++)
    {
      int             sl = ox * 8 / h;
      int             sh = (ox + 1) * 8 / h;
      if (sh <= sl)
	sh = sl + 1;
      /* source bits sl..sh-1, MSB first */
      m[ox] = (uint8_t) ((0xFF >> sl) & (0xFF << (8 - sh)));
    }
  exptab[h][0] = 0;
  for (b = 1; b < 256; b++)
    {
      uint16_t        e = 0;
      for (ox = 0; ox < h; ox++)
	if (b & m[ox])
	  e |= (uint16_t) (0x8000 >> ox);
      exptab[h][b] = e;
    }
  expdone[h] = 1;
}

/* ---------------------------------------------------------------- */
/* the line cache                                                   */

/* ten lines fit in the visible depth range; two spare */
#define NSLOT 12

typedef struct
{
  short           line;         /* text line index, -1 = free  */
  short           h;            /* rendered glyph height       */
  short           col;          /* rendered shade              */
  short           dstx;         /* screen x, 16px aligned      */
  short           w;            /* surface width, multiple of 16 */
  STDL_Surface   *s;
}
CSlot;

static CSlot    slots[NSLOT];

/* 1bpp scratch: 320px x 16 rows, plus the deposit guard */
static uint8_t  bitbuf[(MAPWIDTH / 8) * HMAX + 2];

static void
drop_slot (CSlot * cs)
{
  if (cs->s)
    STDL_FreeSurface (cs->s);
  cs->s = NULL;
  cs->line = -1;
  cs->w = 0;
}

static void
drop_all_slots (void)
{
  int             i;
  for (i = 0; i < NSLOT; i++)
    drop_slot (slots + i);
}

/*
 * Render one text line at glyph height h in colour col.  The text is
 * centred to the exact pixel, but the surface starts on a 16px
 * boundary so the blit runs in phase - the sub-word centring is
 * baked into the bitmap as transparent padding.
 */
static void
render_line (CSlot * cs, const char *s, int h, int col)
{
  int             len = (int) strlen (s);
  int             textw, startx, x0, x1, w, bpr, r, k;

  if (cs->s)
    STDL_FreeSurface (cs->s);
  cs->s = NULL;
  cs->w = 0;
  cs->h = (short) h;
  cs->col = (short) col;
  if (len == 0)
    return;

  textw = len * h;
  startx = (MAPWIDTH - textw) / 2;
  x0 = startx > 0 ? (startx & ~15) : 0;
  x1 = startx + textw;
  if (x1 > MAPWIDTH)
    x1 = MAPWIDTH;
  x1 = (x1 + 15) & ~15;         /* stays <= MAPWIDTH: 320 is aligned */
  w = x1 - x0;
  if (w <= 0)
    return;

  bpr = w / 8;
  ensure_exptab (h);
  memset (bitbuf, 0, (size_t) (bpr * h + 2));
  for (r = 0; r < h; r++)
    {
      uint8_t        *row = bitbuf + r * bpr;
      int             srl = r * 8 / h;
      int             srh = (r + 1) * 8 / h;
      const uint8_t  *fp;
      if (srh <= srl)
	srh = srl + 1;
      for (k = 0; k < len; k++)
	{
	  int             p = startx + k * h - x0;
	  uint16_t        e;
	  uint8_t         fb;
	  int             j;
	  if (p + h <= 0 || p >= w)
	    continue;
	  fp = crawl_font + (unsigned char) s[k] * 8;
	  fb = fp[srl];
	  for (j = srl + 1; j < srh; j++)
	    fb |= fp[j];
	  if (fb == 0)
	    continue;
	  e = exptab[h][fb];
	  if (p < 0)
	    {
	      e = (uint16_t) (e << -p);
	      p = 0;
	    }
	  if (w - p < 16)
	    e &= (uint16_t) (0xFFFFu << (16 - (w - p)));
	  if (e)
	    {
	      /* deposit into a 24-bit window; the |=0 tail bytes are
	         why bitbuf carries two guard bytes */
	      uint32_t        v = (uint32_t) e << (8 - (p & 7));
	      uint8_t        *b = row + (p >> 3);
	      b[0] |= (uint8_t) (v >> 16);
	      b[1] |= (uint8_t) (v >> 8);
	      b[2] |= (uint8_t) v;
	    }
	}
    }
  cs->s = STDL_SurfaceFrom1bpp (bitbuf, w, h, (uint8_t) col, C_BG);
  if (cs->s == NULL)
    return;                     /* out of memory: the line just skips */
  STDL_SetColourKey (cs->s, 1, C_BG);
  cs->w = (short) w;
  cs->dstx = (short) x0;
}

/*
 * Rescaling every line the instant it crosses a height bucket can
 * put two or three scaler runs in one frame, and that frame stalls
 * visibly.  So each frame spends at most one re-render; a line that
 * cannot have it yet is drawn at its old size for one more frame,
 * which is a 1px pop nobody can see.  A line entering the screen has
 * no old size to fall back on and always renders.
 */
static int      rbudget;

static CSlot   *
slot_for (int line, const char *txt, int h, int col)
{
  CSlot          *cs = NULL;
  int             i;

  for (i = 0; i < NSLOT; i++)
    if (slots[i].line == line)
      {
	cs = slots + i;
	if (cs->h != h)
	  {
	    if (rbudget <= 0)
	      return cs;        /* stale size, draw it once more */
	    rbudget--;
	    render_line (cs, txt, h, col);
	    return cs;
	  }
	if (cs->col != col && cs->s)
	  {
	    /* same size, new fade shade: remap in place, which
	       keeps the mask and skips the scaler entirely */
	    uint8_t         map[16];
	    int             c;
	    for (c = 0; c < 16; c++)
	      map[c] = (uint8_t) c;
	    map[cs->col] = (uint8_t) col;
	    STDL_RemapSurface (cs->s, map);
	    cs->col = (short) col;
	  }
	return cs;
      }
  for (i = 0; i < NSLOT; i++)
    if (slots[i].line < 0)
      {
	cs = slots + i;
	cs->line = (short) line;
	render_line (cs, txt, h, col);
	return cs;
      }
  return NULL;                  /* cannot happen with NSLOT > visible */
}

/* upstream's (1200+d)*32/2500 fade, folded onto the palette */
static int
fade_shade (int tc)
{
  if (tc < 6)
    return C_BLUE + 2;          /* barely off the sky        */
  if (tc < 12)
    return C_GREY + 1;
  if (tc < 18)
    return C_GREY;
  return C_WHITE;
}

/*
 * Draw every visible line, maintain the cache, and return the index
 * of the nearest visible line - upstream's `actu`, which the intro
 * script keys its events off.  Returns -1 when nothing is visible.
 *
 * i_fp is the scroll position in 8.8 depth units.  Culling, fade and
 * the height buckets quantise to whole units - they are thresholds
 * and move a line at most one frame - but the projection divides
 * with the fraction kept, so a line's screen position advances as
 * smoothly as the pixel grid allows rather than in tick-sized
 * clumps.
 */
static int
draw_lines (char *lines[], int n, long i_fp)
{
  int             y, actu = -1, ylo = n, yhi = -1;

  rbudget = 1;
  for (y = 0; y < n; y++)
    {
      long            d_fp = (((long) y * TEXTW) << 8) - i_fp;
      long            den_fp;
      int             d = (int) (d_fp >> 8);
      int             tc, sy, h;
      CSlot          *cs;

      if (d <= D_FAR || d >= D_NEAR)
	continue;
      tc = (1200 + d) * 32 / 2500;
      if (tc <= 0)
	continue;
      actu = y;
      den_fp = (1000L << 8) - d_fp; /* 400.0 .. 2200.0 */
      sy = HORIZON + (int) (((PROJ << 8) + den_fp / 2) / den_fp);
      if (sy >= MAPHEIGHT + 20)
	continue;
      h = (int) (GLYPROJ / (den_fp >> 8));
      if (h > HMAX)
	h = HMAX;
      if (h < HMIN)
	h = HMIN;
      cs = slot_for (y, lines[y], h, fade_shade (tc));
      if (y < ylo)
	ylo = y;
      if (y > yhi)
	yhi = y;
      if (cs != NULL && cs->w > 0)
	{
	  /* cs->h, not h: the slot may be a frame behind the bucket */
	  STDL_Rect       src, dst;
	  src.x = 0;
	  src.y = 0;
	  src.w = (uint16_t) cs->w;
	  src.h = (uint16_t) cs->h;
	  dst.x = cs->dstx;
	  dst.y = (int16_t) sy;
	  dst.w = src.w;
	  dst.h = src.h;
	  STDL_BlitSurface (cs->s, &src, backscreen, &dst);
	  DirtyBox (cs->dstx, sy, cs->w, cs->h);
	}
    }
  /* lines that scrolled out give their surface back */
  for (y = 0; y < NSLOT; y++)
    if (slots[y].line >= 0 && (slots[y].line < ylo || slots[y].line > yhi))
      drop_slot (slots + y);
  return actu;
}

/* ---------------------------------------------------------------- */
/* the intro script: upstream's starwars() choreography in whole    */
/* degrees and 8.8 fixed point                                      */

#define RINIT8  47010           /* sqrt(160^2+90^2) << 8             */
#define COLL8   (((ROCKET_RADIUS + BALL_RADIUS) / DIV) << 8)
#define ANGSTEP 77              /* 0.3 deg/tick                      */
#define WIG_F   440             /* 0.03 rad/tick in 8.8 degrees      */
#define WIG_S   220             /* 0.015 rad/tick                    */

static struct
{
  long            r[3], rp[3];  /* ring radius / step, 8.8 px  */
  long            angle;        /* 8.8 degrees, 0..360         */
  long            playr, playp; /* rocket wiggle, 8.8 degrees  */
  int             time0, time1;
  int             playx, playy; /* px; playx==0 = not born yet */
  int             bballx, bbally;
}
st;

static void
script_init (void)
{
  memset (&st, 0, sizeof (st));
  st.r[0] = st.r[1] = st.r[2] = RINIT8;
  st.rp[1] = st.rp[2] = (6 << 8) / 10 / DIV;    /* 0.6/DIV px/tick */
  st.playp = WIG_F;
}

static void
draw_koules_c (int variant, int sdeg, int rpx)
{
  static BitmapType *const bm[3] = { &ball_bitmap,
    &lball_bitmap[0], &lball_bitmap[1]
  };
  int             a;
  for (a = 0; a < 360; a += 60)
    PutBitmap (MAPWIDTH / 2 - BALL_RADIUS / DIV
	       + (int) ((ksin (a + sdeg) * rpx) >> 16),
	       MAPHEIGHT / 2 - BALL_RADIUS / DIV
	       + (int) ((kcos (a + sdeg) * rpx) >> 16),
	       BALL_RADIUS * 2 / DIV, BALL_RADIUS * 2 / DIV, *bm[variant]);
}

/* the six koules condense out of a screenful of dust */
static void
koulescreator_c (int rpx)
{
  int             a, z;
  Effect (S_CREATOR1, next);
  for (a = 0; a < 360; a += 60)
    {
      int             x1 = MAPWIDTH / 2 - (int) ((ksin (a) * rpx) >> 16);
      int             y1 = MAPHEIGHT / 2 - (int) ((kcos (a) * rpx) >> 16);
      for (z = 0; z < CREATOR_PARTS (BALL_RADIUS); z++)
	{
	  int             x = KRAND_N (MAPWIDTH);
	  int             y = KRAND_N (MAPHEIGHT);
	  addpoint (x * DIV * 256, y * DIV * 256,
		    (x1 - x) * DIV * 256 / 100,
		    (y1 - y) * DIV * 256 / 100,
		    ball (KRAND_N (32)), 100);
	}
    }
}

/* and the player out of starlight */
static void
starcreator_c (void)
{
  int             z;
  Effect (S_CREATOR1, next);
  for (z = 0; z < CREATOR_PARTS (ROCKET_RADIUS); z++)
    {
      int             x = KRAND_N (MAPWIDTH);
      int             y = KRAND_N (MAPHEIGHT);
      int             c = KRAND_N (32);
      addpoint (x * DIV * 256, y * DIV * 256,
		(MAPWIDTH / 2 - x) * DIV * 256 / 100,
		(MAPHEIGHT / 2 - y) * DIV * 256 / 100,
		c >= 20 ? C_WHITE : C_GREY + (c >= 10), 100);
    }
}

static void
draw_player_c (int x, int y, int rdeg)
{
  int             lx = x * DIV, ly = y * DIV, x1, y1;
  PutBitmap (x - ROCKET_RADIUS / DIV, y - ROCKET_RADIUS / DIV,
	     ROCKET_RADIUS * 2 / DIV, ROCKET_RADIUS * 2 / DIV,
	     rocket_bitmap[0]);
  x1 = lx + (int) ((ksin (rdeg - 30) * EYE_RADIUS1) >> 16) - EYE_RADIUS;
  y1 = ly + (int) ((kcos (rdeg - 30) * EYE_RADIUS1) >> 16) - EYE_RADIUS;
  PutBitmap (x1 / DIV, y1 / DIV,
	     EYE_RADIUS * 2 / DIV, EYE_RADIUS * 2 / DIV, eye_bitmap[0]);
  x1 = lx + (int) ((ksin (rdeg + 30) * EYE_RADIUS1) >> 16) - EYE_RADIUS;
  y1 = ly + (int) ((kcos (rdeg + 30) * EYE_RADIUS1) >> 16) - EYE_RADIUS;
  PutBitmap (x1 / DIV, y1 / DIV,
	     EYE_RADIUS * 2 / DIV, EYE_RADIUS * 2 / DIV, eye_bitmap[0]);
}

/* under the text: the rings and the descending B_BALL */
static void
script_under (void)
{
  int             z;
  for (z = 0; z < 3; z++)
    {
      if (st.r[z] <= COLL8)
	{
	  st.rp[z] = -(6 << 8) / DIV;
	  Effect (S_COLIZE, next);
	}
      if (st.r[z] < RINIT8)
	draw_koules_c (st.rp[z] > 0 ? z : 0,
		       (int) (st.angle >> 8), (int) (st.r[z] >> 8));
    }
  if (st.bbally > -20 && st.bballx)
    PutBitmap (st.bballx - BBALL_RADIUS / DIV, st.bbally,
	       BBALL_RADIUS * 2 / DIV, BBALL_RADIUS * 2 / DIV, bball_bitmap);
}

/* over it: the player, who is born when the credits say so */
static void
script_over (const CrawlScript * sc, int actu)
{
  if (st.playx)
    draw_player_c (st.playx, st.playy, (int) (st.playr >> 8));
  if (actu == sc->playerline && !st.time0)
    {
      starcreator_c ();
      st.time0 = 1;
    }
}

/* one upstream tick of the choreography */
static void
script_tick (const CrawlScript * sc, int actu)
{
  if (actu >= sc->koulesline && !st.time1)
    {
      koulescreator_c (MAPHEIGHT / 2 - 20);
      st.time1 = 1;
    }
  if (st.time1)
    st.time1++;
  if (st.time1 == 100)
    st.r[0] = (long) (MAPHEIGHT / 2 - 20) << 8;
  if (st.time1 > 100)
    {
      st.r[0] -= st.rp[0];
      st.angle += ANGSTEP;
      if (st.angle >= (360L << 8))
	st.angle -= (360L << 8);
    }
  st.playr += st.playp;
  if (st.playr < -(45L << 8))
    {
      st.playp = WIG_S;
      st.playr = -(45L << 8);
    }
  if (st.playr > (45L << 8))
    {
      st.playp = -WIG_F;
      st.playr = (45L << 8);
    }
  if (actu >= sc->d1line)
    st.r[1] -= st.rp[1];
  if (actu >= sc->d2line)
    st.r[2] -= st.rp[2];
  if (actu >= sc->bline && !st.bballx)
    {
      st.bballx = MAPWIDTH / 2;
      st.bbally = MAPHEIGHT + 30;
    }
  if (st.bballx)
    st.bbally--;
  if (st.bbally > 0 && st.bbally < MAPHEIGHT / 2 + ROCKET_RADIUS / 2)
    {
      if (st.playy == MAPHEIGHT / 2)
	Effect (S_END, next);
      st.playy -= 10;           /* the hero runs away */
    }
  if (st.time0)
    st.time0++;
  if (st.time0 == 100)
    {
      st.playx = MAPWIDTH / 2;
      st.playy = MAPHEIGHT / 2;
      st.playr = 180L << 8;
      st.rp[0] = (3 << 8) / 2 / DIV;    /* 1.5/DIV px/tick */
      Effect (S_CREATOR2, next);
    }
}

/* ---------------------------------------------------------------- */
/* the crawl itself                                                 */

/*
 * Scroll `lines` past in perspective; sc carries the intro's event
 * script, NULL for the plain briefings.  A key press skips - after
 * the key that brought us here has been let go.
 */
void
CrawlText (char *lines[], int n, const CrawlScript * sc)
{
  STDL_Point      stars[NSTARS];
  uint32_t        last;
  long            i_fp, end_fp, tick;
  int             actu = -1, j;

  fadeout ();
  SetScreen (backscreen);
  npoint = 0;
  drop_all_slots ();
  if (sc)
    script_init ();

  /* the starfield lives in the background surface, so the dirty-box
     restores put the stars back for free */
  STDL_FillRect (background, NULL, C_BG);
  for (j = 0; j < NSTARS; j++)
    {
      stars[j].x = (int16_t) KRAND_N (MAPWIDTH);
      stars[j].y = (int16_t) KRAND_N (MAPHEIGHT + 20);
    }
  STDL_Points (background, stars, NSTARS / 2, C_GREY + 1);
  STDL_Points (background, stars + NSTARS / 2, NSTARS / 2, C_GREY);
  DirtyAll ();

  UpdateInput ();
  while (Pressed ())            /* let go of whatever brought us here */
    UpdateInput ();

  if (sc)
    Effect (S_START, 0);
  i_fp = -660L << 8;
  end_fp = ((long) (n + (sc ? 10 : 4)) * TEXTW) << 8;
  tick = -660;
  last = STDL_GetTicks ();

  while (i_fp < end_fp)
    {
      int             a, dt, ran;
      uint32_t        now;

      ErasePoints ();
      RestoreBackground ();
      if (sc)
	script_under ();
      points ();
      a = draw_lines (lines, n, i_fp);
      if (a >= 0)
	actu = a;
      if (sc)
	script_over (sc, actu);
      FlushPoints ();
      CopyToScreen (backscreen);
      fadein1 ();

      /* Advance by real time at upstream's 65 ticks a second, with
         the fraction kept: a steady 20ms frame moves the crawl a
         steady 1.3 ticks instead of alternating 1 and 2, which is
         what used to make it lurch.  17039/1024 is 16.64, ticks<<8
         per millisecond.  The clamp keeps a fade or a dropped frame
         from throwing the crawl forward. */
      now = STDL_GetTicks ();
      dt = (int) (now - last);
      if (dt < 10)
	{
	  STDL_Delay ((uint32_t) (10 - dt));
	  now = STDL_GetTicks ();
	  dt = (int) (now - last);
	}
      last = now;
      if (dt < 1)
	dt = 1;
      if (dt > 120)
	dt = 120;
      i_fp += ((long) dt * 17039L) >> 10;

      /* the choreography still runs in whole upstream ticks */
      ran = 0;
      while (tick < (i_fp >> 8))
	{
	  if (sc)
	    script_tick (sc, actu);
	  tick++;
	  if (ran++)
	    points1 ();         /* the drawn frame already stepped them */
	}

      UpdateInput ();
      if (Pressed ())
	break;
    }

  fadeout ();
  while (Pressed ())
    UpdateInput ();
  drop_all_slots ();
  npoint = 0;
  STDL_FillRect (background, NULL, C_BG);       /* playfield again */
  DirtyAll ();
  tbreak = 1;
}
