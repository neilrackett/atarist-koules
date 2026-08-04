/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  Atari ST / STDL backend                                 *
*----------------------------------------------------------*
*  stdl/init.c  start-up, shutdown and the main entry      *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#include <interface.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef __MINT__
#include <mint/osbind.h>
#endif

#include "../koules.h"
#include "../framebuffer.h"

int             sprite_flags = 0;

extern void     game (void);
extern void     setcustompalette (int, float);
extern void     starwars (void);

/* Cooperative sleep. STDL's clock is the 200Hz system timer, so the
 * resolution is 5ms; upstream only ever asks for whole frames or a
 * whole second. */
void
myusleep (unsigned long usec)
{
  STDL_Delay ((uint32_t) (usec / 1000UL));
}

void
uninitialize (void)
{
  STDL_Quit ();
}

#ifdef __MINT__
static long
freeram (void)
{
  return Malloc (-1L);
}
#else
static long
freeram (void)
{
  return 0;
}
#endif

static int
initialize (void)
{
  long            heap;
  uint32_t        vflags;

  if (STDL_Init (STDL_INIT_VIDEO | STDL_INIT_JOYSTICK) != 0)
    {
      fprintf (stderr, "STDL_Init: %s\n", STDL_GetError ());
      return -1;
    }

  /*
   * A second screen page is what stops the objects flickering: the
   * frame is drawn where nobody can see it and appears in one flip.
   * It costs 32KB, which a 512KB machine has not got once the
   * background surface, the sprites and the samples are in - and the
   * same Malloc(-1) probe that picks the sprite format answers this
   * one, before the mode is set rather than after.
   */
  heap = freeram ();
  vflags = (heap >= DOUBLEBUF_HEAP) ? STDL_DOUBLEBUF : 0;
  backscreen = STDL_SetVideoMode (MAPWIDTH, MAPHEIGHT + 20, 4, vflags);
  if (backscreen == NULL)
    {
      fprintf (stderr, "SetVideoMode: %s\n", STDL_GetError ());
      return -1;
    }
  fprintf (stderr, "koules: heap %ld bytes, %s\n", heap,
	   (backscreen->flags & STDL_DOUBLEBUF)
	   ? "double buffered" : "single buffered (low memory)");
  SetScreen (backscreen);

  background = STDL_CreateSurface (MAPWIDTH, MAPHEIGHT + 20);
  if (background == NULL)
    {
      fprintf (stderr, "background: %s\n", STDL_GetError ());
      return -1;
    }
  starbackground = NULL;

  /* The stick drives player one and the menus; upstream's keyboard
   * code then needs no joystick branch at all. */
  STDL_JoyKeyMapping (STDLK_UP, STDLK_DOWN, STDLK_LEFT, STDLK_RIGHT,
		      STDLK_RETURN);
  STDL_JoyKeyEmulation (1);
  return 0;
}

int
main (int argc, char *argv[])
{
  uint32_t        t0, t1;
  long            free0;

  (void) argc;
  (void) argv;                  /* GEMDOS hands the desktop no argv */

  nrockets = 1;
  drawpointer = 0;

  srand (time (NULL));

  if (initialize () != 0)
    return 1;

  free0 = freeram ();
#ifdef KOULES_NOPRESHIFT
  sprite_flags = 0;
#else
  sprite_flags = (free0 >= PRESHIFT_HEAP) ? STDL_PRESHIFT : 0;
#endif
  fprintf (stderr, "koules: free %ld bytes, sprites %s\n", free0,
	   sprite_flags ? "pre-shifted" : "plain (low memory)");
  t0 = STDL_GetTicks ();

  setcustompalette (0, 1);
  create_bitmap ();
  drawbackground ();

  t1 = STDL_GetTicks ();
  fprintf (stderr, "koules: assets built in %lu ms, free %ld bytes\n",
	   (unsigned long) (t1 - t0), freeram ());

#ifdef SOUND
  /* After the sprites, deliberately: they have first claim on the
     heap, and whatever is left decides whether the samples fit or
     the YM has to stand in for them. */
  t0 = STDL_GetTicks ();
  init_sound ();
  t1 = STDL_GetTicks ();
  fprintf (stderr, "koules: sound ready in %lu ms\n",
	   (unsigned long) (t1 - t0));
  sound = sndinit;
#ifdef KOULES_SOUNDTEST
  {
    extern void     sound_selftest (void);
    sound_selftest ();
    uninitialize ();
    return 0;
  }
#endif
#endif

  gamemode = MENU;
#ifdef KOULES_STARTLEVEL
  /* diagnostic builds only: jump straight to a busy level so the
     frame cost of a full playfield can be measured */
  maxlevel = lastlevel = KOULES_STARTLEVEL - 1;
#endif

  keys[0][0] = STDLK_UP;
  keys[0][1] = STDLK_DOWN;
  keys[0][2] = STDLK_LEFT;
  keys[0][3] = STDLK_RIGHT;

  keys[1][0] = STDLK_w;
  keys[1][1] = STDLK_s;
  keys[1][2] = STDLK_a;
  keys[1][3] = STDLK_d;

  keys[2][0] = STDLK_i;
  keys[2][1] = STDLK_k;
  keys[2][2] = STDLK_j;
  keys[2][3] = STDLK_l;

  keys[3][0] = STDLK_KP8;
  keys[3][1] = STDLK_KP5;
  keys[3][2] = STDLK_KP4;
  keys[3][3] = STDLK_KP6;

  keys[4][0] = STDLK_t;
  keys[4][1] = STDLK_g;
  keys[4][2] = STDLK_f;
  keys[4][3] = STDLK_h;

  starwars ();
  game ();

  uninitialize ();
  return 0;
}
