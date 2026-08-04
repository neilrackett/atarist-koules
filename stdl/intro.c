/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  Atari ST / STDL backend                                 *
*----------------------------------------------------------*
*  stdl/intro.c  level briefings                           *
*----------------------------------------------------------*
* Upstream scrolls these into the distance in Star Wars     *
* perspective, drawn with font.c: a 1000-line vector font   *
* that builds every glyph out of lines and arcs and needs   *
* sin/cos/atan per frame.  On an 8MHz 68000 that is a       *
* per-pixel renderer running behind a 65fps target, which   *
* is the wrong shape of work for a planar machine, so the   *
* ST build shows the same words as a page instead of a      *
* scroller.  Nothing else in the game depends on it -       *
* gameplan.c only wants the briefing shown and the key      *
* press waited for.                                         *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include <interface.h>
#include <stdio.h>
#include <string.h>

#include "../koules.h"
#include "../physics.h"
#include "../text.h"

#define PAGELINES 16
#define NSTARS    100

/*
 * One screenful: text, a scatter of stars, then wait for a key.
 * Returns 1 if the player asked to get on with it, which skips the
 * rest of the briefing - upstream's crawl aborts on a key too.
 */
static int
onepage (char *lines[], int n)
{
  uint32_t        t0;
  int             j, skipped = 0;
  STDL_Point      stars[NSTARS];

  TextPage ((const char *const *) lines, n);

  for (j = 0; j < NSTARS; j++)
    {
      stars[j].x = KRAND_N (MAPWIDTH);
      stars[j].y = KRAND_N (MAPHEIGHT + 20);
    }
  STDL_Points (backscreen, stars, NSTARS, C_GREY);
  CopyToScreen (backscreen);    /* nothing flips again while it is up */

  fadein ();
  UpdateInput ();
  while (Pressed ())            /* let go of whatever brought us here */
    UpdateInput ();
  t0 = STDL_GetTicks ();
  while (STDL_GetTicks () - t0 < 6000)
    {
      UpdateInput ();
      if (Pressed ())
	{
	  skipped = 1;
	  break;
	}
      STDL_Delay (20);
    }
  while (Pressed ())
    UpdateInput ();
  fadeout ();
  return skipped;
}

/* Show a block of lines, a page at a time. */
static void
page (char *lines[], int n)
{
  char           *shown[PAGELINES];
  int             i, nshow = 0;

  fadeout ();
  for (i = 0; i < n; i++)
    {
      /* upstream's "..." lines are scroller beats, not content */
      if (lines[i] == NULL || lines[i][0] == '\0'
	  || !strcmp (lines[i], "..."))
	continue;
      shown[nshow++] = lines[i];
      if (nshow == PAGELINES)
	{
	  nshow = 0;
	  if (onepage (shown, PAGELINES))
	    break;
	  fadeout ();
	}
    }
  if (nshow)
    onepage (shown, nshow);
  tbreak = 1;
}

void
clearpoints ()
{
  int             i;
  for (i = 0; i < MAXPOINT; i++)
    point[i].time = 0;
  npoint = 0;
}

/* The opening crawl.  Same treatment: the title page, then play. */
void
starwars ()
{
  page (text, TEXTSIZE);
}

void
outro1 ()
{
  page (text1, TEXTSIZE1);
}

void
outro2 ()
{
  page (text2, TEXTSIZE2);
}

void
intro_intro ()
{
  page (introtext, INTROSIZE);
}

void
hole_intro ()
{
  page (holetext, HOLESIZE);
}

void
inspector_intro ()
{
  page (inspectortext, INSPECTORSIZE);
}

void
bball_intro ()
{
  page (bballtext, BBALLSIZE);
}

void
bbball_intro ()
{
  page (bbballtext, BBBALLSIZE);
}

void
maghole_intro ()
{
  page (magholetext, MAGSIZE);
}

void
spring_intro ()
{
  page (springtext, SPRINGTSIZE);
}

void
thief_intro ()
{
  page (thieftext, THIEFSIZE);
}

void
ttool_intro ()
{
  page (ttooltext, TTOOLSIZE);
}

void
finder_intro ()
{
  page (findertext, FINDERSIZE);
}

void
lunatic_intro ()
{
  page (lunatictext, LUNATICSIZE);
}
