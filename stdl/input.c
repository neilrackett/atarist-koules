/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  Atari ST / STDL backend                                 *
*----------------------------------------------------------*
*  stdl/input.c  keyboard and joystick                     *
***********************************************************/

#include <interface.h>
#include <stdbool.h>
#include <stdio.h>

/*
 * STDL keysyms are numerically identical to SDL 1.2's, so the key
 * tables the game saves in its rc file and the defaults in stdl/init.c
 * are the same numbers upstream used.
 */
static const uint8_t *keystate;
static int      nkeys;
static int      sawdown;
static int      last_pressed;

static void
refresh (void)
{
  keystate = STDL_GetKeyState (&nkeys);
}

/*
 * Upstream's UpdateInput() polls exactly one event per call, which
 * on SDL means the queue drains over several frames.  The ST queue is
 * short and the game polls once a frame, so drain it completely: a
 * dropped key-up would leave a rocket thrusting for ever.
 */
void
UpdateInput (void)
{
  STDL_Event      e;

  sawdown = 0;
  while (STDL_PollEvent (&e))
    {
      switch (e.type)
	{
	case STDL_KEYDOWN:
	  last_pressed = e.key.keysym.sym;
	  sawdown = 1;
	  break;
	case STDL_KEYUP:
	  last_pressed = 0;
	  break;
	default:
	  break;
	}
    }
  refresh ();
}

int
GetKey (void)
{
  int             key = last_pressed;

  last_pressed = 0;
  return key;
}

/*
 * "Has the player touched anything": the key state scanned rather
 * than a press/release count, so a missed key-up cannot wedge the
 * briefing and pause loops that spin on this, plus a flag for keys
 * that went down and up inside a single pump - a tap short enough to
 * fall between two polls still counts, which matters when the poll
 * interval is a whole ST frame.
 */
bool
Pressed (void)
{
  int             i;

  if (sawdown)
    return true;
  if (keystate == NULL)
    return false;
  for (i = 0; i < nkeys; i++)
    if (keystate[i])
      return true;
  return false;
}

bool
IsPressed (int key)
{
  if (keystate == NULL || key < 0 || key >= nkeys)
    return false;
  return keystate[key] != 0;
}

int
IsPressedUp (void)
{
  return IsPressed (STDLK_UP);
}

int
IsPressedDown (void)
{
  return IsPressed (STDLK_DOWN);
}

int
IsPressedLeft (void)
{
  return IsPressed (STDLK_LEFT);
}

int
IsPressedRight (void)
{
  return IsPressed (STDLK_RIGHT);
}

int
IsPressedEnter (void)
{
  return IsPressed (STDLK_RETURN) || IsPressed (STDLK_KP_ENTER);
}

int
IsPressedEsc (void)
{
  return IsPressed (STDLK_ESCAPE);
}

int
IsPressedH (void)
{
  return IsPressed (STDLK_h);
}

int
IsPressedP (void)
{
  return IsPressed (STDLK_p);
}
