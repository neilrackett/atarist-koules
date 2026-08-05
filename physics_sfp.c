/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  physics_sfp.c - upstream float physics on the SFP-004   *
*----------------------------------------------------------*
* The KOULSFP4 build.  Where KOULES.TOS replaced upstream's *
* float simulation with 16.16 fixed point because the       *
* 68000 has no FPU, this build puts the floats back and     *
* hands the expensive part to a memory-mapped 68881/68882   *
* (an SFP-004 card, or a Mega STE with a 68882 and a GAL    *
* decoder) through the atarist-sfp004 library.  Not because *
* it is faster than 16.16 -- it is not -- but because the   *
* machine has a coprocessor socket and it would be rude not *
* to.                                                       *
*                                                           *
* sim/physics_float.c is included, not copied: it is the    *
* pristine upstream reference the fixed build is diffed     *
* against, and it must stay byte-identical.  The only edit  *
* is textual: sqrt -> ksqrt below renames every call (and   *
* math.h's declaration, included via koules.h, which        *
* becomes ksqrt's prototype).  That catches the sqrt in     *
* normalize() -- the collision and force loops' hot path -- *
* plus the spring distance, the gravity distance and the    *
* creator cloud, which is every transcendental in the       *
* simulation.  One FSQRT replaces hundreds of cycles of     *
* soft float; adds and multiplies stay in libgcc, where     *
* the CIR dialog overhead would eat the win.                *
*                                                           *
* No FPU fitted?  ksqrt falls back to libm and the whole    *
* build is simply upstream's float physics, soft.  The      *
* sfp004_active() gate also covers the frames before        *
* stdl/init.c has armed the dispatch.                       *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#define sqrt ksqrt
#include "sim/physics_float.c"
#undef sqrt

#include "atari_sfp004.h"

extern double   sqrt (double);	/* math.h's declaration was renamed */

double
ksqrt (double x)
{
  if (sfp004_active ())
    return (double) sfp004_sqrt ((float) x);
  return sqrt (x);
}
