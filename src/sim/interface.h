/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  src/sim/interface.h - headless stand-in for a backend   *
*                                                          *
*  koules.h does #include <interface.h> to pick up the      *
*  backend's types and playfield geometry.  The numeric     *
*  harness and the on-target benchmark link no backend at   *
*  all, so this supplies just enough for the simulation     *
*  translation units to compile.  Nothing here is called.   *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#ifndef _KOULES_SIM_INTERFACE_H
#define _KOULES_SIM_INTERFACE_H

#include "gamedim.h"

typedef void   *BitmapType;
typedef void   *VScreenType;
typedef void   *RawBitmapType;

#define COLORS 256
typedef struct
{
  struct
  {
    unsigned char   red, green, blue;
  }
  color[COLORS];
}
Palette;

#define EYE_RADIUS 6		/* DIV == 2 */
#define MOUSE_RADIUS 4

#endif /* _KOULES_SIM_INTERFACE_H */
