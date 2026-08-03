/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  Atari ST / STDL backend                                 *
*----------------------------------------------------------*
*  stdl/interface.h  common definitions for the STDL       *
*                    backend                               *
***********************************************************/
/* Changes for Atari ST/STE with STDL                      *
 *  Copyright(c)2026 by Neil Rackett                       *
 ************************NR*********************************/

#ifndef _KOULES_STDL_INTERFACE_H
#define _KOULES_STDL_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#include <stdl/stdl.h>

/*
 * Upstream's BitmapType and RawBitmapType are the same type in every
 * backend (create_bitmap() assigns the result of CompileBitmap() --
 * which takes a raw bitmap -- straight into a BitmapType), so both
 * are one struct here.  It starts life as a planar surface that
 * BSetPixel() pokes at load time and is converted, once, into a
 * pre-shifted sprite: the surface is freed the moment CompileBitmap()
 * has consumed it.
 */
typedef struct KBitmap
{
  STDL_Surface   *surf;         /* only while being built           */
  STDL_Sprite    *spr;          /* the drawable form                */
  short           w, h;
}
KBitmap;

typedef KBitmap *BitmapType;
typedef KBitmap *RawBitmapType;
typedef STDL_Surface *VScreenType;

/*
 * The ST has sixteen colours, so Palette is sixteen entries wide.
 * Components stay on upstream's 0..63 VGA scale: cmap.c's fade loops
 * do their own arithmetic on them and are used unchanged.
 */
#define COLORS 16
typedef struct
{
  struct
  {
    uint8_t         red;
    uint8_t         green;
    uint8_t         blue;
  }
  color[COLORS];
}
Palette;

extern VScreenType background;
extern VScreenType backscreen;
extern VScreenType starbackground;

/*
 * The particle buffer.  points() calls SMySetPixel once per live
 * particle -- a few hundred times in an explosion -- so it is an
 * inline that appends to the array STDL_PointsC is handed at the end
 * of the frame, not a call into stdl/draw.c.  On an 8MHz 68000 the
 * four argument pushes and the jsr/rts cost more than the three
 * stores they exist to make.
 *
 * KPT_MAX is MAXPOINT, which koules.h defines further down than this
 * header is included; stdl/draw.c checks the two agree.
 */
#define KPT_MAX 512
extern STDL_Point kpt_xy[KPT_MAX];
extern uint8_t  kpt_col[KPT_MAX];
extern int      kpt_n;

static __inline__ void
SMySetPixel (VScreenType screen, int x, int y, int c)
{
  (void) screen;
  if (kpt_n < KPT_MAX)
    {
      kpt_xy[kpt_n].x = (int16_t) x;
      kpt_xy[kpt_n].y = (int16_t) (y >> 8);
      kpt_col[kpt_n] = (uint8_t) c;
      kpt_n++;
    }
}

#include "gamedim.h"

#define EYE_RADIUS 6            /* DIV == 2 */
#define MOUSE_RADIUS 4

RawBitmapType   CreateBitmap (const int, const int);
BitmapType      CompileBitmap (const int, const int, const RawBitmapType);
void            ClearScreen (void);
void            SetScreen (VScreenType screen);
void            CopyToScreen (VScreenType);
void            CopyVSToVS (VScreenType, VScreenType);

void            UpdateInput (void);
int             GetKey (void);
bool            Pressed (void);
bool            IsPressed (int);
int             IsPressedDown (void);
int             IsPressedEnter (void);
int             IsPressedEsc (void);
int             IsPressedH (void);
int             IsPressedLeft (void);
int             IsPressedP (void);
int             IsPressedRight (void);
int             IsPressedUp (void);

void            BSetPixel (RawBitmapType bitmap, int, int, int);
int             SGetPixel (int, int);
void            SPutPixel (int, int, int);
void            SSetPixel (int, int, int);
void            DrawText (int, int, char *);
void            DrawBlackMaskedText (int, int, char *);
void            DrawWhiteMaskedText (int, int, char *);
void            DrawRectangle (int, int, int, int, int);
void            HLine (int, int, int, int);
void            Line (int, int, int, int, int);
void            Line1 (int, int, int, int, int);
void            PutBitmap (const int, const int, const int, const int,
			   const BitmapType);

void            EnableClipping (void);
void            DisableClipping (void);

void            WaitRetrace (void);
void            SetPalette (Palette * pal);

void            myusleep (unsigned long);
void            uninitialize (void);

void            fadeout (void);
void            fadein (void);
void            fadein1 (void);

/*
 * Additions for the ST backend.  Upstream repaints the whole
 * background surface over the whole back screen every frame (90kpx
 * for 2-10kpx of moving content), which an 8MHz 68000 cannot afford;
 * here every draw that lands on the screen records its bounding box
 * and RestoreBackground() repaints just those boxes.
 *
 * Particles bypass that: they are single pixels, so instead of a
 * rectangle each they are buffered and erased wholesale with one
 * batched span call in the flat playfield colour (the playfield
 * cannot be anything but flat: check_limit() keeps every particle
 * inside y < MAPHEIGHT).
 */
void            RestoreBackground (void);
void            SuppressDirty (int on);
void            ClearOverlay (void);
void            ErasePoints (void);
void            FlushPoints (void);
void            StatusBar (const char *lives, const char *scores);
void            DirtyAll (void);

/* Whole-screen text page used by the level intros (stdl/intro.c). */
void            TextPage (const char *const *lines, int nlines);

#endif /* _KOULES_STDL_INTERFACE_H */
