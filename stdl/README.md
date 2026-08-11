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

| File          | Lines | Contents                                                                       |
| ------------- | ----- | ------------------------------------------------------------------------------ |
| `draw.c`      | 737   | sprites, primitives, page flipping, dirty tracking, particles, status bar      |
| `sound.c`     | 445   | the seven effects, on STE DMA where affordable and on the YM elsewhere         |
| `init.c`      | 201   | start-up, shutdown, the page, pre-shift and sample RAM decisions               |
| `intro.c`     | 197   | level briefings as paged text                                                  |
| `interface.h` | 180   | the backend seam                                                               |
| `input.c`     | 166   | keyboard and joystick                                                          |
| `font8x8.h`   | 2592  | SDL_gfx's 8x8 font, carried verbatim (LGPL)                                    |

## Building

STDL is a submodule at `lib/stdl`, pinned to a release tag, so clone with
it:

```
git clone --recurse-submodules https://github.com/neilrackett/atarist-koules.git
```

In a clone that already exists, `git submodule update --init` fetches it.
The game then cross-compiles with `m68k-atari-mint-gcc` via
[atarist-toolkit-docker](https://github.com/sidecartridge/atarist-toolkit-docker),
in one command - `libstdl.a` is built from the submodule as part of it:

```
stcmd make
```

The root `Makefile` picks between the per-platform makefiles - `make atari`
(the default), `make sim` for the host physics harness, and `make clean` for
both - so `stcmd make -f Makefile.atari` is equivalent if you prefer being
explicit.

This produces `dist/KOULES.TOS` plus the converted sound assets. `dist/` doubles
as a Hatari GEMDOS drive:

```
hatari --machine st dist/KOULES.TOS
```

`dist/KDEBUG.TOS` is the same game with a per-phase console report, and
`dist/SFXTEST.TOS` plays the seven effects in order for audio checking.

## Status

Playable. Measured on an emulated plain 8MHz ST with 1MB; the game caps
itself at 25 fps (`VFTIME`):

| Scene                                | fps | render | of which the flip |
| ------------------------------------ | --- | ------ | ----------------- |
| Level 1                              | 20  | 26ms   | 11ms              |
| Level 60, steady                     | 19  | 32ms   | 5ms               |
| Level 60, explosion (~250 particles) | 10  | 70ms   | 10ms              |
| Menu                                 | 19  | 16ms   | 14ms              |

The flip is a wait for the raster, so it is idle time, not work: the same
scenes single-buffered render in 15, 27, 60 and 3ms and run at exactly the
same frame rates, because what the flip waits through is time the frame
would otherwise have spent in `STDL_Delay` hitting its 40ms deadline. The
one place it does cost frames is a machine already far past that deadline -
a Mega STE at level 60 with five players goes from 11 fps to 9 - and there
the trade is nine complete frames against eleven half-drawn ones.

Explosions are the worst case and were once far worse: drawing the particle
field through one batched `STDL_PointsC` call, and merging plane words as
longs so the inner loop stops spilling registers, took that frame's
rendering from 124ms to 69ms. Particles are still about half of it.

A Mega STE is comfortably faster throughout. A 512KB ST plays level 1 at the
same rate, having fallen back to non-pre-shifted sprites and to a single
screen page - which is the one configuration where the objects still
flicker, there being no 32KB to spare for the second page.

One binary covers every machine, choosing at run time:

| Machine             | Screen           | Sprites             | Sound               |
| ------------------- | ---------------- | ------------------- | ------------------- |
| STE / Mega STE, 1MB | two pages        | pre-shifted (~82KB) | 6258 Hz DMA samples |
| STE, 512KB          | one page         | plain               | YM                  |
| Plain ST, 1MB+      | two pages        | pre-shifted         | YM                  |
| Plain ST, 512KB     | one page         | plain               | YM                  |

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

**Two screen pages.** A frame erases what moved and redraws it somewhere
else, and on an 8MHz ST that takes about as long as one sweep of the raster.
Drawn straight onto the visible screen - which is what this port did
originally - every object the beam passes between its erase and its redraw is
simply missing from that sweep, so a busy level start showed half its koules
in an occasional frame. The frame is now drawn into a page nobody is looking
at and shown with one VBL-synced `STDL_Flip`, which is atomic. Measured over
consecutive captures of a level start, 22 frames in 43 were missing content
before and none are now.

The page costs 32KB, so `init.c` asks for it only when the `Malloc(-1)` probe
says there is room, exactly as it does for the pre-shifted sprites. A 512KB
machine has 41KB free at that point and stays single-buffered - and keeps the
flicker; from 1MB up there is half a megabyte spare and it does not.

**Dirty rectangles.** Upstream copies the entire background over the back
buffer every frame - about 90,000 pixels to move 2-10,000 pixels of content.
It already maintains a persistent background surface, so the compositor
records each object's bounding box and restores only those. The status bar
redraws only when it changes.

Each page keeps its own list, because the page about to be drawn into is two
frames old rather than one: what has to be put back is a property of the page,
not of the frame. That keeps the restore exactly the size it was
single-buffered, where restoring the union of the last two frames into every
page would have doubled it. The same split applies to everything else that
survives between frames - the particle field, the status bar and the menu's
selection frame each track what is on which page.

**Particles** are drawn as a batch through `STDL_PointsC` - one call for the
whole field, colour per point - and erased against the flat playfield, so they
need no save-under and no rectangle each. Drawing them with XOR so a second
pass erases them was tried and rejected: it is only marginally cheaper, and
`explosion()` emits every fragment at the same pixel, so a fresh burst would
speckle where the colours interfere.

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
  what keeps a settled one free of any per-frame drawing at all.

## Licence

Koules is Copyright (C) 1995-1998 Jan Hubicka and Kamil Toman, under the
[GNU General Public License version 2 or later](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html);
see `COPYING` in the repository root. The Atari ST changes are under the same
terms.

STDL itself is LGPL-2.1, and `font8x8.h` is from SDL_gfx under the LGPL.
