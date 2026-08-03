/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  gamedim.h - compile-time playfield geometry             *
*----------------------------------------------------------*
* The Atari ST port runs one resolution only: the original *
* svgalib "-s" mode (320x200x256 -> 320x180 playfield with *
* a 640x360 logical game space).  Values verified against  *
* svgalib/init.c case 's' and sdl/init.c case 's' of the   *
* upstream tree.                                           *
*                                                          *
* These MUST be #defines rather than the upstream globals: *
* every "/ DIV" against a runtime int becomes a __divsi3   *
* library call on a 68000, and there are dozens of them in *
* the draw path.  As constants they fold to a shift.       *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#ifndef _KOULES_GAMEDIM_H
#define _KOULES_GAMEDIM_H

#define GAMEWIDTH   640         /* logical simulation space */
#define GAMEHEIGHT  360
#define MAPWIDTH    320         /* pixels actually drawn     */
#define MAPHEIGHT   180
#define DIV         2           /* logical -> pixel divisor  */

/* Upstream computes this per object per axis per frame as
   (GAMEWIDTH / 640.0 + 1) / 2 -- two double divisions and a
   multiply.  With GAMEWIDTH fixed at 640 it is exactly 1.0,
   so move_objects() loses the multiply altogether. */
#define MOVE_SCALE_IS_ONE 1

#endif /* _KOULES_GAMEDIM_H */
