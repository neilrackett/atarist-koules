/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  gamedim.h - playfield geometry                          *
*----------------------------------------------------------*
* The width is the original svgalib "-s" mode's (320 drawn *
* pixels, a 640 logical game space).  Values verified      *
* against svgalib/init.c case 's' and sdl/init.c case 's'  *
* of the upstream tree.                                    *
*                                                          *
* The width and DIV MUST be #defines rather than the       *
* upstream globals: every "/ DIV" against a runtime int    *
* becomes a __divsi3 library call on a 68000, and there    *
* are dozens of them in the draw path.  As constants they  *
* fold to a shift.                                         *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#ifndef _KOULES_GAMEDIM_H
#define _KOULES_GAMEDIM_H

#define GAMEWIDTH   640         /* logical simulation space */
#define MAPWIDTH    320         /* pixels actually drawn     */
#define DIV         2           /* logical -> pixel divisor  */

/*
 * The height is the screen's, less the 20-row status bar under the
 * playfield: 180 rows, a 360-high game space, on an ST's 200 lines;
 * 220 and 440 on a TT's or a Falcon's 240 (STDL_RESPONSIVE_HEIGHT),
 * as upstream's own full-size mode had a taller field (640x460).
 * stdl/init.c sets it once, after the mode set and before anything is
 * sized or placed from it (kmapheight_set, gamedim.c); the benches and
 * the host sims keep 180. A variable, so what is worked out from it
 * that is not a power of two is worked out there once: MAPHEIGHT / 3,
 * the horizon of the crawl and of the 3D text, would otherwise be a
 * __divsi3 call a line.
 */
#define MAPHEIGHT_ST   180
#define MAPHEIGHT_TALL 220
extern short    kmapheight, kmapthird;
#define MAPHEIGHT   kmapheight
#define GAMEHEIGHT  (MAPHEIGHT * DIV)
#define MAPTHIRD    kmapthird   /* MAPHEIGHT / 3 */
void            kmapheight_set (int h);

/* Upstream computes this per object per axis per frame as
   (GAMEWIDTH / 640.0 + 1) / 2 -- two double divisions and a
   multiply.  With GAMEWIDTH fixed at 640 it is exactly 1.0,
   so move_objects() loses the multiply altogether. */
#define MOVE_SCALE_IS_ONE 1

#endif /* _KOULES_GAMEDIM_H */
