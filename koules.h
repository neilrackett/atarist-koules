/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  C1995 JAHUSOFT                                          *
*        Jan Hubicka                                       *
*        Dukelskych Bojovniku 1944                         *
*        390 03 Tabor                                      *
*        Czech Republic                                    *
*        Phone: 0041-361-32613                             *
*        eMail: hubicka@limax.paru.cas.cz                  *
*----------------------------------------------------------*
*   Copyright(c)1995,1996 by Jan Hubicka.See README for    *
*                    licence details.                      *
*----------------------------------------------------------*
*  koules.h                                                *
***********************************************************/
/* Changes for joystick "accelerate by deflection"         *
 *  (c) 1997 by Ludvik Tesar (Ludv\'{\i}k Tesa\v{r})       *
 ************************LT*********************************/

#ifndef __KOULE_INCLUDED___
#define __KOULE_INCLUDED___
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <fcntl.h>
#ifndef M_PI			/*hp ansi c stuff */
#define M_PI 3.14
#endif
#ifdef JOYSTICK
#include "joystick.h"
#endif
#if defined(SOUND)||defined(NAS_SOUND)||defined(RSOUND)
#include "sound.h"
#endif
#ifndef HAVEUSLEEP
#define usleep myusleep
extern void     myusleep (unsigned long);
#endif

/*do not confuse compiler when function is not required */
#if defined(__GNUC__)&&!defined(ONLYANSI)
#define CONST const
#define INLINE inline
#else
#define INLINE
#define CONST
#endif
#define DUMMY do { } while (0)

extern int      nomouse;
#include <interface.h>
#include "krand.h"

/*
 * Upstream calls rand() a hundred-odd times a frame and mintlib's
 * costs 2450 cycles a call on a 68000 (measured -- see krand.h).
 * Redirect the lot; nothing in the game needs a better generator
 * than a xorshift.  rand()%N is redirected too, because the 32 bit
 * modulo is its own ~1000 cycle library call.
 */
#define rand()  krand ()
#define srand(s) ksrand ((unsigned long) (s))

#define MENUTIME 5



#define back(x) (32 + x)
#define ball(x) (64 + x)
#define rocket(x) (96 + x)


#define PLAY_X1 0
#define PLAY_Y1 0




/*
 * Angles.  The reference build keeps upstream's radians so it can
 * be diffed against the original; the ST build makes RAD() the
 * identity and works in whole degrees, indexing a sine table.
 * ROTSTEP stays exactly 10 degrees either way, so a rocket still
 * has exactly 36 headings.
 */
#ifdef KOULES_FLOAT
#define RAD(n)  ((float)(n)/180.0*M_PI)
#define ROTSTEP RAD(10)
#else
#define RAD(n)  (n)
#define ROTSTEP 10
#endif


#define BALL_RADIUS 8
#define BBALL_RADIUS 16
#define APPLE_RADIUS 32
#define INSPECTOR_RADIUS 14
#define LUNATIC_RADIUS EYE_RADIUS
#define HOLE_RADIUS 12
#define ROCKET_RADIUS 14
#define EYE_RADIUS1 10
#define SPRINGSIZE (4*BBALL_RADIUS)
#define SPRINGSTRENGTH (BBALL_RADIUS/2)



#define NTRACKS 4
#define NTRACKS 4
#define ROCKET 1
#define BALL 2


#define LBALL 3
#define CREATOR 4
#define HOLE 5
#define BBALL 6
#define APPLE 7
#define INSPECTOR 8
#define EHOLE 9
#define LUNATIC 10


#define MAXOBJECT 255
/*
 * Was 4000, and every frame walked all 4000 slots whether or not
 * they held anything: 36.75ms on an 8MHz ST, measured, against a
 * 40ms frame budget.  point[] is now a compacted active list --
 * point[0..npoint) are the live ones -- so the sweep costs what
 * the particles cost.  512 is enough for two simultaneous
 * explosions plus rocket exhaust; beyond that the oldest slot is
 * recycled, which is what upstream's rotating cursor did anyway.
 */
#define MAXPOINT (512)
#define MAXROCKETS 5


#define L_ACCEL 'A'
#define L_GUMM 'M'
#define L_THIEF 'T'
#define L_FINDER 'G'
#define L_TTOOL 'S'
#define A_ADD 0.13
#define M_ADD 0.8
#define NLETTERS 5


#define LETTER 1024


#define S_START 0
#define S_END 1
#define S_COLIZE 2
#define S_DESTROY_BALL 3
#define S_DESTROY_ROCKET 4
#define S_CREATOR1 5
#define S_CREATOR2 6

#define C_REMOTE 0
#define C_KEYBOARD 1
#define C_RKEYBOARD 2
#define C_JOYSTICK1 3
#define C_JOYSTICK2 4
#define C_MOUSE 5

#define DEATHMATCH 0
#define COOPERATIVE 1


#define NSAMPLES 7

#define MENU 1
#define KEYS 2
#define GAME 3
#define JOY 4
#define WAIT 5
#define PREGAME 6


#define next			/*((++cit)>=NTRACKS?cit=1:cit) */


/*
 * oval_t is every scalar the simulation carries.  The ST build
 * makes it 16.16 fixed point; -DKOULES_FLOAT restores upstream's
 * float so the two can be run side by side (see sim/).
 */
#ifdef KOULES_FLOAT
typedef float   oval_t;
#define OVAL(f)   ((oval_t)(f))          /* compile-time literal */
#define OVI(i)    ((oval_t)(i))          /* runtime int */
#define OVDIV(a,b) ((oval_t)(a) / (oval_t)(b))
#define OVMULDIV(v,a,b) ((oval_t)((v) * (double)(a) / (b)))
#define OV2D(v)   ((double)(v))
#define OVROT     float
#else
#include "fixed.h"
typedef fix_t   oval_t;
#define OVAL(f)   FIX(f)                 /* compile-time literal */
#define OVI(i)    FIXI(i)                /* runtime int */
#define OVDIV(a,b) (FIXI(a) / (b))
/* v * a / b with a,b small integers; no 64 bit, no float */
#define OVMULDIV(v,a,b) ((fix_t)(((v) / (b)) * (a) + ((((v) % (b)) * (a)) / (b))))
#define OV2D(v)   ((double)(v) / 65536.0)
#define OVROT     int
#endif

typedef struct
  {
    int             type;
    int             thief;
    int             ctype;
    int             live;
    int             time;
    int             score;
    int             lineto;
    oval_t          x;
    oval_t          y;
    oval_t          fx;		/*forces */
    oval_t          fy;
    OVROT           rotation;	/*for rockets: radians, or whole
				  degrees in the fixed point build */
    int             live1;	/*backup for rockets */
    oval_t          M;
    int             radius;
    oval_t          accel;
    char            letter;
/* B ****LT**** */
#ifdef JOYSTICK
   float          joymulx;    /* multiply x cootdinate by this to obtai number between <0,1> */
   float          joymuly;    /* multiply y ... */
   float          joythresh;  /* minimum how should be joystick deflected for acceleration <0,1> */
#endif
/* B ****LT**** */
  }
Object;
typedef struct
  {
    int             x, y, xp, yp, time, color;
  }
Point;
extern int      PlaySound (int s);
#ifdef NETSUPPORT
#define play_sound1(p1) (!server?play_sound(p1):PlaySound(p1))
#else
#define play_sound1(p1) play_sound(p1)
#endif

#ifdef SOUND
#define Effect(p1,p2) (sound?play_sound1(p1):0)
#else
#if defined(NAS_SOUND)||defined(RSOUND)
#define Effect(p1,p2) (sound?play_sound1(p1):0)
#define SOUND
#else
#define Effect(p1,p2) (server?PlaySound(p1):0)
#endif
#endif

struct control
  {
    int             type;
    double          jx, jy;
    int             mx, my;
    int             mask;
  };
#ifndef VARIABLES_HERE
extern oval_t   ROCKET_SPEED;
extern oval_t   BALL_SPEED;
extern oval_t   BBALL_SPEED;
extern oval_t   SLOWDOWN;
extern oval_t   GUMM;

extern oval_t   BALLM;
extern oval_t   LBALLM;
extern oval_t   BBALLM;
extern oval_t   APPLEM;
extern oval_t   INSPECTORM;
extern oval_t   LUNATICM;
extern oval_t   ROCKETM;


extern int      dosprings;
extern int      difficulty;
extern int      randsprings;
extern int      nobjects;
extern int      drawpointer;
extern int      textcolor;
extern int      nrockets;
extern Object   object[MAXOBJECT];
extern Point    point[MAXPOINT];
extern int      gameplan;
extern int      rotation[MAXROCKETS];
extern char     control[MAXROCKETS];
extern struct control controls[5];
extern int      lastlevel, maxlevel;
#ifdef NETSUPPORT
extern int      client, server;
#endif
#ifdef SOUND
extern int      sndinit;
#endif

extern int      mouseplayer;
#ifdef JOYSTICK
extern int      joystickplayer[2];
extern int      joystickdevice[2];
extern int      calibrated[2];
extern int      center[2][2];
/* B ****LT**** */
/* coordinates are multiplied by this number before computing of speed */
extern float    joystickmul[2];
/* default value for "accel by fire button" : */
#define JOYMUL1 0.0
/* default value for "accel by deflection" : */
#define JOYMUL2 1.5
/* joystickthresh is lower threshold for movement of joystick (something between 0 and 1)*/
extern float    joystickthresh[2];
/* E ****LT**** */
#endif


extern VScreenType physicalscreen;
extern VScreenType backscreen;
extern VScreenType background;
extern VScreenType starbackground;
/*extern int      cit; */
extern int      gamemode;
extern int      tbreak;

extern int      a_bballs, a_rockets, a_balls, a_holes, a_apples, a_inspectors,
                a_lunatics, a_eholes;

extern int      keys[5][4];
extern int      sound;

extern BitmapType bball_bitmap, apple_bitmap, inspector_bitmap, mouse_bitmap,
                lunatic_bitmap, lball_bitmap[NLETTERS], circle_bitmap,
                hole_bitmap, ehole_bitmap, ball_bitmap, eye_bitmap[MAXROCKETS],
                rocket_bitmap[MAXROCKETS];
extern unsigned char rocketcolor[5];


#endif


extern void     addpoint (CONST int, CONST int, CONST int, CONST int, CONST int, CONST int);
extern void     accel (CONST int, CONST oval_t);
extern void     creators_points (int, int, int, int);
extern void     explosion (CONST int, CONST int, CONST int, CONST int, CONST int);
extern void     destroy (CONST int);
extern void     creator (CONST int);
extern void     creator_rocket (CONST int);
extern void     uninitialize ();
extern void     draw_menu (CONST int);
extern void     draw_joy (CONST int);
extern void     init_menu ();
extern void     menu_keys ();

extern void     draw_keys (int);
extern void     keys_keys ();
extern void     joy_keys ();
extern void     gameplan_init ();

extern void     update_game ();
extern void     init_objects ();
extern void     outro1 ();
extern void     outro2 ();
extern void     clearpoints ();
extern void     intro_intro ();
extern void     lunatic_intro ();
extern void     spring_intro ();
extern void     thief_intro ();
extern void     finder_intro ();
extern void     ttool_intro ();
extern void     hole_intro ();
extern void     inspector_intro ();
extern void     bball_intro ();
extern void     bbball_intro ();
extern void     maghole_intro ();
extern void     fadeout ();
extern void     fadein ();
extern void     load_rc ();
extern void     save_rc ();
extern int      allow_finder ();
extern int      find_possition (oval_t *, oval_t *, CONST int);
extern int      radius (CONST int);
extern oval_t   M (CONST int);
extern int      create_letter (void);
#ifndef NETSUPPORT
#define client 0
#define server 0
#endif

#endif
