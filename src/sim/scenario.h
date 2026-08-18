/***********************************************************
*  src/sim/scenario.h - deterministic scenario, shared     *
*  by the numeric harness and the on-target benchmark      *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#ifndef _KOULES_SIM_SCENARIO_H
#define _KOULES_SIM_SCENARIO_H

extern void     sim_setup (int n, unsigned int seed);
extern void     sim_frame (int frame);
extern void     sim_place (int i);
extern int      sim_respawn (void);

#endif
