/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  Atari ST / STDL backend                                 *
*----------------------------------------------------------*
*  stdl/sound.c  the seven effects, on DMA or on the YM    *
***********************************************************/

/*
 * Upstream's sound.c drove a forked sound-server process over a
 * pipe, which TOS cannot do, so this is a replacement for the same
 * five-call interface (sound.h): init_sound, play_sound,
 * maybe_play_sound, sound_completed, kill_sound.
 *
 * Two backends, chosen at run time from the hardware and the free
 * heap, exactly the way stdl/init.c already decides between
 * pre-shifted and plain sprites:
 *
 *   DMA  STE/Mega STE with the RAM to hold the samples: the seven
 *        upstream sounds resampled to 6258 Hz (the .WAV files in
 *        assets, built by tools/mksounds.sh), played one at a time
 *        straight out of RAM by the hardware.
 *   YM   everything else - a plain 8 MHz ST has no DMA sound at
 *        all, and a 512K machine has no room for 120K of samples.
 *        Seven step effects approximate the same seven events on
 *        the YM2149.
 *
 * Neither backend costs frame time, and that is the whole reason
 * they look like this.  The obvious implementation - SDL_mixer's
 * four mixed channels over STDL_OpenAudio - was built first and
 * measured at 360-755 ms of CPU per second on an 8 MHz STE at
 * level 60 (36-75%, and still 25-43% on a 16 MHz Mega STE),
 * because a ring device has to software-mix and resample every
 * sample the DMA eats.  In a game already down to 4-11 fps that is
 * unaffordable, so the sample path is monophonic instead:
 * STDL_PlaySample points the DMA at the buffer and the hardware
 * reads it with no further CPU at all.  Measured cost afterwards is
 * 0-10 ms/s, i.e. nothing.  The YM path was always free - STDL
 * steps effects from the VBL sound tick, a few register writes at
 * 50 Hz.
 *
 * Being monophonic needs arbitration, because the game fires
 * several effects in one frame (an explosion is a destroy plus a
 * collision plus a creator).  A new sound wins unless something
 * more important is still inside its opening SOUND_HOLD ms; after
 * that anything may cut in.  So every event is always heard - its
 * attack, which is the part you actually recognise - and no long
 * sample can block the soundscape behind it.
 */

#include <interface.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef __MINT__
#include <mint/osbind.h>
#endif

#include "../koules.h"

#ifdef SOUND

/*
 * The sample device rate.  6258 Hz is the lowest exact STE DMA rate
 * and the only one that does not inflate the set: upstream's sounds
 * are 8000 Hz, so this is a small downsample (157K -> 120K) rather
 * than the 1.56x blow-up 12517 Hz would cost, and 8000 Hz source
 * has nothing above 4 kHz to protect anyway.
 */
#define SAMPLE_RATE 6258

/*
 * Free store the DMA backend needs: the seven samples are 122842
 * bytes and are played in place, so that is also the peak.  Round
 * up for allocator overhead and fragmentation.  A 1M machine has
 * ~495K free once the sprites are built, a 512K machine ~36K - so
 * this threshold is what actually splits the two configurations.
 */
#define SAMPLE_HEAP 150000L

/*
 * How long a sample keeps the right to refuse a less important one.
 * Long enough that every effect is recognisable, short enough that
 * the 4-second explosions cannot mute the game behind them.
 */
#define SOUND_HOLD 300

#define NSOUNDS 7

/* Indexed by S_START..S_CREATOR2 (koules.h), the same order as
 * FILENAME[] in the upstream sound servers. */
static const char *const soundfile[NSOUNDS] = {
  "START.WAV",			/* S_START           */
  "END.WAV",			/* S_END             */
  "COLIZE.WAV",			/* S_COLIZE          */
  "DESTROY1.WAV",		/* S_DESTROY_BALL    */
  "DESTROY2.WAV",		/* S_DESTROY_ROCKET  */
  "CREATOR1.WAV",		/* S_CREATOR1        */
  "CREATOR2.WAV"		/* S_CREATOR2        */
};

/*
 * Priority, high wins.  Your own rocket exploding is the one sound
 * that must always be heard; the collision tick is the one that
 * fires dozens of times a second and may always be dropped.
 */
static const signed char priority[NSOUNDS] = {
  4,				/* S_START           */
  4,				/* S_END             */
  1,				/* S_COLIZE          */
  3,				/* S_DESTROY_BALL    */
  5,				/* S_DESTROY_ROCKET  */
  2,				/* S_CREATOR1        */
  2				/* S_CREATOR2        */
};

enum
{ SND_OFF, SND_DMA, SND_YM };
static int      backend = SND_OFF;

/* ------------------------------------------------------------------ */
/* YM approximations                                                   */

/*
 * One per event.  Tone effects are short arpeggios; the two
 * explosions are noise with a volume envelope, at different noise
 * periods so they stay distinguishable (12 is a bright hiss, 28 a
 * deep rumble).  A noise effect ignores its period array except for
 * the value 0, which means "silent step", so those arrays are 1s.
 */

/* rising fanfare - a level begins */
static const uint16_t start_p[6] = {
  STDL_YM_PERIOD (262), STDL_YM_PERIOD (330), STDL_YM_PERIOD (392),
  STDL_YM_PERIOD (523), STDL_YM_PERIOD (659), STDL_YM_PERIOD (784)
};
static const STDL_Sfx start_fx = { start_p, NULL, 6, 12, 60, 0 };

/* falling figure - a level ends */
static const uint16_t end_p[6] = {
  STDL_YM_PERIOD (659), STDL_YM_PERIOD (523), STDL_YM_PERIOD (392),
  STDL_YM_PERIOD (330), STDL_YM_PERIOD (262), STDL_YM_PERIOD (196)
};
static const STDL_Sfx end_fx = { end_p, NULL, 6, 12, 80, 0 };

/* the collision tick: two frames, quiet, high - it has to survive
 * being fired continuously without becoming a drone */
static const uint16_t colize_p[2] = {
  STDL_YM_PERIOD (1568), STDL_YM_PERIOD (1047)
};
static const STDL_Sfx colize_fx = { colize_p, NULL, 2, 7, 20, 0 };

/* a ball goes: short bright burst */
static const uint16_t noise_p[14] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};
static const uint8_t dball_v[8] = { 14, 13, 11, 9, 7, 5, 3, 1 };
static const STDL_Sfx dball_fx = { noise_p, dball_v, 8, 14, 30, 12 };

/* a rocket goes: longer, deeper, louder */
static const uint8_t drocket_v[14] = {
  15, 15, 14, 14, 13, 12, 11, 10, 8, 7, 5, 4, 2, 1
};
static const STDL_Sfx drocket_fx = { noise_p, drocket_v, 14, 15, 40, 28 };

/* a creator spawns: low warble */
static const uint16_t creator1_p[8] = {
  STDL_YM_PERIOD (147), STDL_YM_PERIOD (185), STDL_YM_PERIOD (147),
  STDL_YM_PERIOD (196), STDL_YM_PERIOD (165), STDL_YM_PERIOD (220),
  STDL_YM_PERIOD (185), STDL_YM_PERIOD (247)
};
static const STDL_Sfx creator1_fx = { creator1_p, NULL, 8, 11, 50, 0 };

/* short blip - also the menu's key click */
static const uint16_t creator2_p[3] = {
  STDL_YM_PERIOD (880), STDL_YM_PERIOD (1319), STDL_YM_PERIOD (1047)
};
static const STDL_Sfx creator2_fx = { creator2_p, NULL, 3, 10, 30, 0 };

static const STDL_Sfx *const ymfx[NSOUNDS] = {
  &start_fx, &end_fx, &colize_fx, &dball_fx, &drocket_fx,
  &creator1_fx, &creator2_fx
};

/* ------------------------------------------------------------------ */

/* the samples, signed 8-bit at SAMPLE_RATE, played where they lie */
static uint8_t *sample[NSOUNDS];
static uint32_t samplelen[NSOUNDS];

/* what is on the DMA now, and when it started */
static signed char dmaprio = -1;
static uint32_t dmastart;

/* what priority owns each of the three YM voices */
static signed char voiceprio[3];

/* Upstream's one-shot guard: maybe_play_sound() plays only if the
 * previous one has been retired with sound_completed(). */
static char     sound_flags[NSOUNDS];

static long
freeram (void)
{
#ifdef __MINT__
  return Malloc (-1L);
#else
  return 0;
#endif
}

/*
 * Pick a YM voice for a sound of priority p: a free one, else the
 * voice holding the least important sound, and only if that is
 * strictly less important than p.  Returns -1 to drop the sound,
 * which is what should happen when a collision tick arrives while
 * three explosions are already playing.
 */
static int
pick_voice (int p)
{
  int             v, worst = -1;
  signed char     worstp = 127;

  for (v = 0; v < 3; v++)
    {
      if (!STDL_SfxActive (v))
	return v;
      if (voiceprio[v] < worstp)
	{
	  worstp = voiceprio[v];
	  worst = v;
	}
    }
  return (worstp < p) ? worst : -1;
}

/*
 * Load one sample.  The .WAV files are already unsigned 8-bit mono
 * at exactly SAMPLE_RATE, so all that is left is the sign flip the
 * DMA wants, done in place - no second buffer, no resampling, and
 * the file's own allocation is the one the hardware reads from.
 */
static int
load_sample (int k, const char *file)
{
  STDL_AudioSpec  spec;
  uint8_t        *buf;
  uint32_t        len, j;

  if (STDL_LoadWAV (file, &spec, &buf, &len) == NULL)
    {
      fprintf (stderr, "koules: %s: %s\n", file, STDL_GetError ());
      return -1;
    }
  if (spec.freq != SAMPLE_RATE || spec.channels != 1
      || spec.format != STDL_AUDIO_U8)
    {
      fprintf (stderr, "koules: %s: not %d Hz u8 mono\n", file,
	       SAMPLE_RATE);
      STDL_FreeWAV (buf);
      return -1;
    }
  for (j = 0; j < len; j++)
    buf[j] ^= 0x80;
  sample[k] = buf;
  samplelen[k] = len;
  return 0;
}

/* ------------------------------------------------------------------ */

void
init_sound (void)
{
  const STDL_MachineInfo *mach;
  long            freemem;
  int             k;

  for (k = 0; k < NSOUNDS; k++)
    sound_flags[k] = 0;
  for (k = 0; k < 3; k++)
    voiceprio[k] = -1;

  mach = STDL_GetMachineInfo ();
  freemem = freeram ();

  /*
   * Samples need both the DMA hardware and the RAM.  Ask in that
   * order so the console line says which one was missing.
   */
  if (mach == NULL || !mach->is_ste)
    {
      fprintf (stderr, "koules: no DMA sound hardware, YM effects\n");
    }
  else if (freemem < SAMPLE_HEAP)
    {
      fprintf (stderr, "koules: only %ld bytes free, YM effects\n",
	       freemem);
    }
  else
    {
      for (k = 0; k < NSOUNDS; k++)
	if (load_sample (k, soundfile[k]) != 0)
	  break;
      if (k == NSOUNDS)
	{
	  backend = SND_DMA;
	  fprintf (stderr, "koules: %d Hz DMA samples, %ld free\n",
		   SAMPLE_RATE, freeram ());
	}
      else
	{
	  /* a partial set is worse than none: give the RAM back and
	     let the YM cover every event instead */
	  while (k-- > 0)
	    {
	      STDL_FreeWAV (sample[k]);
	      sample[k] = NULL;
	    }
	}
    }

  if (backend == SND_OFF)
    {
      /* The YM2149 is on every ST ever made, so this cannot fail
         for want of hardware. */
      backend = SND_YM;
      fprintf (stderr, "koules: YM2149 effects on 3 voices\n");
    }

  sndinit = 1;
}

int
play_sound (int k)
{
  int             slot;

  if (k < 0 || k >= NSOUNDS)
    return 0;

  switch (backend)
    {
    case SND_DMA:
      /* one at a time: refuse only while something more important
         is still inside its opening SOUND_HOLD ms */
      if (!STDL_SamplePlaying ())
	dmaprio = -1;
      else if (priority[k] < dmaprio
	       && STDL_GetTicks () - dmastart < SOUND_HOLD)
	break;
      if (STDL_PlaySample (sample[k], samplelen[k], SAMPLE_RATE) == 0)
	{
	  dmaprio = priority[k];
	  dmastart = STDL_GetTicks ();
	}
      break;

    case SND_YM:
      /* three voices, and voice A is the one STDL hands to a
         speaker tone, so effects share all three equally here */
      slot = pick_voice (priority[k]);
      if (slot < 0)
	break;
      voiceprio[slot] = priority[k];
      STDL_PlaySfx (ymfx[k], slot);
      break;

    default:
      break;
    }
  return 0;
}

void
maybe_play_sound (int k)
{
  if (k < 0 || k >= NSOUNDS || (sound_flags[k] & 1))
    return;
  sound_flags[k] |= 1;
  play_sound (k);
}

void
sound_completed (int k)
{
  if (k >= 0 && k < NSOUNDS)
    sound_flags[k] &= ~1;
}

#ifdef KOULES_SOUNDTEST
/*
 * Diagnostic build only (-DKOULES_SOUNDTEST): play the seven
 * effects in id order, each to completion, with a second of silence
 * between them and a console marker before each.  The markers line
 * the recording up with the ids, so a capture of the emulator's
 * output can be checked effect by effect - the game itself never
 * fires them in a known order.
 */
void
sound_selftest (void)
{
  int             k, guard;

  for (k = 0; k < NSOUNDS; k++)
    {
      fprintf (stderr, "SFXTEST %d %s\n", k, soundfile[k]);
      play_sound (k);
      for (guard = 0; guard < 800; guard++)
	{
	  int             busy = (backend == SND_DMA)
	    ? STDL_SamplePlaying () : STDL_SfxActive (-1);
	  if (!busy)
	    break;
	  STDL_Delay (10);
	}
      STDL_Delay (1000);
    }
  fprintf (stderr, "SFXTEST done\n");
}
#endif

void
kill_sound (void)
{
  switch (backend)
    {
    case SND_DMA:
      STDL_StopSample ();
      dmaprio = -1;
      break;
    case SND_YM:
      STDL_StopSfx (-1);
      break;
    default:
      break;
    }
}

#endif /* SOUND */
