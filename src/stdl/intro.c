/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  Atari ST / STDL backend                                 *
*----------------------------------------------------------*
*  stdl/intro.c  intros and outros                         *
*----------------------------------------------------------*
* These used to be static text pages: upstream's crawl is   *
* drawn with font.c, a vector font that projects and clips  *
* every stroke per frame, which an 8MHz 68000 cannot do     *
* behind a 65fps loop.  stdl/crawl.c re-renders the crawl   *
* around scaled system-font billboards instead, so the      *
* perspective scroller is back; this file is just the       *
* routing.  gameplan.c only wants each briefing shown and   *
* a key press honoured, and a key skips the crawl exactly   *
* as it dismissed the pages.                                *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include <interface.h>

#include "../koules.h"
#include "../physics.h"
#include "../text.h"

void
clearpoints ()
{
  int             i;
  for (i = 0; i < MAXPOINT; i++)
    point[i].time = 0;
  npoint = 0;
}

/* The opening crawl, with upstream's full choreography: the koules
 * condense, the player is born, the rings close in, the B_BALL
 * arrives and the hero runs away. */
void
starwars ()
{
  CrawlScript     sc;
  sc.koulesline = KOULESLINE;
  sc.playerline = PLAYERLINE;
  sc.d1line = D1LINE;
  sc.d2line = D2LINE;
  sc.bline = BLINE;
  clearpoints ();
  CrawlText (text, TEXTSIZE, &sc);
}

void
outro1 ()
{
  clearpoints ();
  CrawlText (text1, TEXTSIZE1, NULL);
}

void
outro2 ()
{
  clearpoints ();
  CrawlText (text2, TEXTSIZE2, NULL);
}

void
intro_intro ()
{
  clearpoints ();
  CrawlText (introtext, INTROSIZE, NULL);
}

void
hole_intro ()
{
  clearpoints ();
  CrawlText (holetext, HOLESIZE, NULL);
}

void
inspector_intro ()
{
  clearpoints ();
  CrawlText (inspectortext, INSPECTORSIZE, NULL);
}

void
bball_intro ()
{
  clearpoints ();
  CrawlText (bballtext, BBALLSIZE, NULL);
}

void
bbball_intro ()
{
  clearpoints ();
  CrawlText (bbballtext, BBBALLSIZE, NULL);
}

void
maghole_intro ()
{
  clearpoints ();
  CrawlText (magholetext, MAGSIZE, NULL);
}

void
spring_intro ()
{
  clearpoints ();
  CrawlText (springtext, SPRINGTSIZE, NULL);
}

void
thief_intro ()
{
  clearpoints ();
  CrawlText (thieftext, THIEFSIZE, NULL);
}

void
ttool_intro ()
{
  clearpoints ();
  CrawlText (ttooltext, TTOOLSIZE, NULL);
}

void
finder_intro ()
{
  clearpoints ();
  CrawlText (findertext, FINDERSIZE, NULL);
}

void
lunatic_intro ()
{
  clearpoints ();
  CrawlText (lunatictext, LUNATICSIZE, NULL);
}
