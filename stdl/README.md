# Koules for the Atari ST

The Atari ST / STE backend for [Koules](https://github.com/lkundrak/koules),
built on [STDL](https://github.com/neilrackett/atarist-stdl), by
[Neil Rackett](https://neilrackett.com/atarist).

## Introduction

Koules keeps every backend behind one seam - the ~25 functions declared in
`interface.h` - and has historically had four implementations (svgalib, X11,
SDL, OS/2 DIVE). This directory is a fifth, written against STDL so the game
draws in ST interleaved planar with no chunky backbuffer and no c2p pass
anywhere.

Everything above the seam is upstream's: the game logic, level generator,
menus and physics model are unchanged in structure. What the port replaces is
the arithmetic (float to 16.16 fixed point), the palette (256 colours to 16)
and the compositor (full repaint to dirty rectangles).

## Layout

| File           | Lines | Contents                                                              |
| -------------- | ----- | --------------------------------------------------------------------- |
| `draw.c`       |   539 | sprites, primitives, dirty tracking, the particle field, status bar   |
| `sound.c`      |   445 | the seven effects, on STE DMA where affordable and on the YM elsewhere |
| `intro.c`      |   196 | level briefings as paged text                                         |
| `init.c`       |   190 | start-up, shutdown, the pre-shift and sample RAM decisions            |
| `input.c`      |   166 | keyboard and joystick                                                 |
| `interface.h`  |   141 | the backend seam                                                      |
| `font8x8.h`    |  2592 | SDL_gfx's 8x8 font, carried verbatim (LGPL)                           |

## Building

Cross-compiles with `m68k-atari-mint-gcc` via
[atarist-toolkit-docker](https://github.com/sidecartridge/atarist-toolkit-docker).
The toolkit container only mounts the folder it starts in, so run it from the
parent of both checkouts:

```
ST_WORKING_FOLDER=$(realpath ..) stcmd make -C atarist-koules -f Makefile.atari
```

This produces `dist/KOULES.TOS` plus the converted sound assets. `dist/` doubles
as a Hatari GEMDOS drive:

```
hatari --machine st dist/KOULES.TOS
```

`dist/KDEBUG.TOS` is the same game with a per-phase console report, and
`dist/SFXTEST.TOS` plays the seven effects in order for audio checking.

## Status

Playable. Measured on an emulated plain 8MHz ST with 1MB, physics and render
split from the on-screen readout:

| Scene                          | fps          | physics | render |
| ------------------------------ | ------------ | ------- | ------ |
| Level 1                        | 25 (the cap) |     1ms |   16ms |
| Level 60, steady               | 15           |    18ms |   38ms |
| Level 60, explosion (~250 pts) | 6-7          |    16ms |  118ms |
| Menu                           | 25           |     1ms |    1ms |

A Mega STE runs level 60 at 21-25 fps and explosions at 11-13. A 512KB ST
holds 25 fps at level 1.

One binary covers every machine, choosing at run time:

| Machine                | Sprites             | Sound                |
| ---------------------- | ------------------- | -------------------- |
| STE / Mega STE, 1MB    | pre-shifted (~82KB) | 6258 Hz DMA samples  |
| STE, 512KB             | plain               | YM                   |
| Plain ST, any RAM      | 1MB pre-shifted     | YM                   |

## How the port works

**Fixed point.** The 68000 has no FPU, and upstream's physics promotes to
`double`, so gcc's soft-float made the simulation 18-45x over its frame budget
- 1120ms per frame at 20 objects against a 40ms target. The whole simulation
is now 16.16 fixed point with a table-driven sine and an integer square root,
which is 16-28x faster and fits. `make -f Makefile.atari floatcheck` proves no
soft-float helper survives in the binary.

**Sixteen colours.** Upstream builds seven 32-entry gradient ramps plus white
and uses all of them at once. The shading is heavily oversampled at this
resolution - an 8x8 ball spends 17 distinct shades on 37 lit pixels - so three
shades per sphere reads correctly and what is really lost is hue count, not
smoothness. The colour key sits at index 0 rather than 15, because nothing
inside a ball is ever drawn in the background colour; that buys a whole extra
ramp entry.

**Dirty rectangles.** Upstream copies the entire background over the back
buffer every frame - about 90,000 pixels to move 2-10,000 pixels of content.
It already maintains the persistent background surface that `STDL_Dirty`
expects, so the compositor records each object's bounding box and restores only
those. The status bar redraws only when it changes.

**Particles** are drawn as a batch through `STDL_Points` and erased against the
flat playfield, so they need no save-under and no rectangle each.

**Sound** is a one-shot DMA read where the hardware and the RAM allow it, and
YM step effects everywhere else. Mixing in software was measured at 36-75% of
an 8MHz CPU, which this game cannot afford, so the sample path is deliberately
monophonic and arbitrated by priority.

## Not ported

- **Network play** - no sockets on single-tasking TOS.
- **Five players on one keyboard** - needs around 24 distinct hues; the 16
  colour budget supports one or two players.
- **The starwars scroller** - `font.c` renders its glyphs as vector lines and
  arcs with trigonometry per frame, which is a per-pixel renderer by another
  name. The same briefings appear as paged text instead, and every entry point
  is preserved so `gameplan.c` is untouched.
- **Objects animating behind menus** - the menu is a cached overlay, which is
  what keeps it at 25 fps.

## Licence

Koules is Copyright (C) 1995-1998 Jan Hubicka and Kamil Toman, under the
[GNU General Public License version 2 or later](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html);
see `COPYING` in the repository root. The Atari ST changes are under the same
terms.

STDL itself is LGPL-2.1, and `font8x8.h` is from SDL_gfx under the LGPL.
