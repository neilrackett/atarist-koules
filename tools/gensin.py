#!/usr/bin/env python3
"""
Regenerate sintab.c.

Angles in the fixed point build are whole degrees, so the table is
indexed directly and RAD() is the identity -- ROTSTEP stays exactly
10 degrees and a rocket keeps exactly 36 headings, which a
power-of-two angle unit could not give.

Stored in 1.15 as shorts (2340 bytes) and covering -360..+719, so
fixsin()/fixcos() need no range reduction for any angle the game
forms: rotation is 0..359, eye placement adds +/-30, the exhaust
spray adds -22..+22, and fixcos() adds a further 90.

    python3 tools/gensin.py > src/sintab.c
"""

import math

BIAS = 360          # index 0 is -360 degrees
SPAN = 1080 + 90    # -360 .. +719, plus the cosine's 90 of headroom

print('''/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  src/sintab.c - sine table for the fixed point build     *
*                                                          *
*  GENERATED - see tools/gensin.py.  Do not hand edit.     *
*                                                          *
*  Angles are integer degrees.  The table is stored in 1.15*
*  (value * 32767) and covers -360..+719 so that fixsin()  *
*  and fixcos() need no range reduction for any angle the  *
*  game forms: rotation is always 0..359, and the callers  *
*  add at most +/-30 (eye placement) or -22..+134          *
*  (exhaust spray, cos index adds a further 90).           *
***********************************************************/

#include "fixed.h"

const short fix_sin_tab[1080 + 90] = {''')

row = []
for i in range(SPAN):
    row.append('%6d' % round(math.sin(math.radians(i - BIAS)) * 32767.0))
    if len(row) == 10:
        print('  ' + ', '.join(row) + ',')
        row = []
if row:
    print('  ' + ', '.join(row) + ',')
print('};')
