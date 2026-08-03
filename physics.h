/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  physics.h - the headless simulation                     *
*----------------------------------------------------------*
* Everything in physics.c runs without touching the screen *
* so it can be linked into the numeric harness and the     *
* on-target benchmark without dragging in a backend.       *
***********************************************************/

#ifndef _KOULES_PHYSICS_H
#define _KOULES_PHYSICS_H

#include "fixed.h"

/* one simulation step, in the order game() runs them */
extern void     update_forces (void);
extern void     colisions (void);
extern void     move_objects (void);
extern void     check_limit (void);
extern void     update_values (void);

extern void     points1 (void);		/* particle step, no drawing */

/* accel() and explosion() are declared in koules.h, which types
   them against oval_t so both builds agree */
extern void     explosion (const int, const int, const int, const int,
			   const int);

/* supplied by gameplan.c in the game, by sim/stubs.c in the
   harness and the benchmark */
extern int      allow_finder (void);
extern int      create_letter (void);

extern int      npoint;

/* normalise a heading to 0..359 so the sine table index stays in
   range however long a player leans on the rotate key */
#define ANGWRAP(a) do {				\
    while ((a) < 0) (a) += 360;			\
    while ((a) >= 360) (a) -= 360;		\
  } while (0)

/* Number of explosion fragments upstream's
 *   for (i = 0; i < RAD(360); i += RAD(360)*DIV*DIV/r/r/M_PI)
 * loop produces, which is ceil(r*r*pi/4).  3217/4096 is pi/4 to
 * seven digits and reproduces every radius the game uses
 * (6,8,10,12,14,16,32) exactly.  Getting this count right is not
 * cosmetic: each fragment consumes three rand() calls, so a
 * different count would desynchronise the RNG stream and make the
 * float and fixed builds diverge for reasons unrelated to the
 * arithmetic.
 */
#define EXPLOSION_PARTS(r)  ((((r) * (r) * 3217) + 4095) >> 12)
#define CREATOR_PARTS(r)    (((r) * (r) * 3217) >> 12)

#endif /* _KOULES_PHYSICS_H */
