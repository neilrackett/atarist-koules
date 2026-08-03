/*
 * sound.h - Platform Independant Sound Support - Dec. 1994
 *
 * Copyright 1994 Sujal M. Patel (smpatel@wam.umd.edu)
 * Conditions in "copyright.h"
 *
 * Restored for the Atari ST port: the interface is upstream's, only
 * the empty parameter lists have become prototypes so the backend
 * compiles clean under -Wall -Wextra.  stdl/sound.c implements it.
 */

#if defined(SOUND) || defined(NAS_SOUND) || defined(RSOUND)

void            init_sound (void);	/* Init Sound System                          */
int             play_sound (int k);	/* Play a Sound                               */
void            maybe_play_sound (int k);	/* Play sound if the last 'k' sound_completed */
void            sound_completed (int k);	/* Complete a sound 'k'                       */
void            kill_sound (void);	/* Terminate a sound unpredictably :)         */

#endif /* SOUND */
