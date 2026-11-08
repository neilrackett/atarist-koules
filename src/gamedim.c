/***********************************************************
*  gamedim.c - the playfield's height, see gamedim.h       *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include "gamedim.h"

short           kmapheight = MAPHEIGHT_ST;
short           kmapthird = MAPHEIGHT_ST / 3;

void
kmapheight_set (int h)
{
  kmapheight = (short) h;
  kmapthird = (short) (h / 3);
}
