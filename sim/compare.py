#!/usr/bin/env python3
"""
Diff two runs of sim/dump (upstream float vs 16.16 fixed point).

The two are not expected to agree bit for bit -- the point of the
exercise is to find out *how much* they disagree and whether the
disagreement stays bounded.  Koules is a chaotic n-body-ish
simulation, so any difference at all eventually reorders
collisions; what matters is that positions stay inside the arena,
objects stay on plausible trajectories, and nothing tunnels.

usage: compare.py FLOAT.csv FIXED.csv [label]
"""

import csv
import sys


def load(path):
    frames = {}
    with open(path) as fh:
        for row in csv.DictReader(fh):
            frames.setdefault(int(row['frame']), {})[int(row['obj'])] = row
    return frames


def stats_of(frames):
    """Aggregate character of a run: the things a player would
    actually notice, as opposed to exact trajectories."""
    import math
    speeds = []
    radii = []
    deaths = 0
    live_frames = 0
    total_slots = 0
    prev_live = {}
    cx, cy = 320.0, 180.0
    for f in sorted(frames):
        for i, row in frames[f].items():
            live = int(row['live'])
            total_slots += 1
            if live:
                live_frames += 1
                fx, fy = float(row['fx']), float(row['fy'])
                speeds.append(math.hypot(fx, fy))
                radii.append(math.hypot(float(row['x']) - cx,
                                        float(row['y']) - cy))
            if i in prev_live and prev_live[i] and not live:
                deaths += 1
            prev_live[i] = live
    speeds.sort()
    return {
        'mean speed': sum(speeds) / len(speeds) if speeds else 0.0,
        'median speed': speeds[len(speeds) // 2] if speeds else 0.0,
        'p95 speed': speeds[int(len(speeds) * 0.95)] if speeds else 0.0,
        'max speed': speeds[-1] if speeds else 0.0,
        'mean dist from centre': sum(radii) / len(radii) if radii else 0.0,
        'live fraction': live_frames / max(total_slots, 1),
        'deaths': float(deaths),
    }


def main():
    fa, fb, label = sys.argv[1], sys.argv[2], (sys.argv[3] if len(sys.argv) > 3 else '')
    A, B = load(fa), load(fb)

    nframes = min(len(A), len(B))
    stats = []          # (frame, max|dx|, mean|dx|, max|dfx|, discrete mismatches)
    first_discrete = None
    ever_outside = []

    for f in range(nframes):
        ra, rb = A[f], B[f]
        dmax = dmean = dfmax = 0.0
        n = 0
        mism = 0
        for i in sorted(set(ra) & set(rb)):
            a, b = ra[i], rb[i]
            dx = abs(float(a['x']) - float(b['x']))
            dy = abs(float(a['y']) - float(b['y']))
            d = max(dx, dy)
            dfx = max(abs(float(a['fx']) - float(b['fx'])),
                      abs(float(a['fy']) - float(b['fy'])))
            dmax = max(dmax, d)
            dfmax = max(dfmax, dfx)
            dmean += d
            n += 1
            if a['type'] != b['type'] or a['live'] != b['live']:
                mism += 1
            # anything outside the arena means the object escaped,
            # which is the failure mode that would actually matter
            for src, row in (('f', a), ('x', b)):
                px, py = float(row['x']), float(row['y'])
                if not (-64 <= px <= 640 + 64 and -64 <= py <= 360 + 64):
                    ever_outside.append((f, i, src, px, py))
        if mism and first_discrete is None:
            first_discrete = f
        stats.append((f, dmax, dmean / max(n, 1), dfmax, mism))

    print('=== %s (%d frames, %d objects) ===' % (label, nframes, len(A[0])))
    print(' frame   max|dpos|   mean|dpos|   max|dforce|  type/live mismatches')
    for f, dmax, dmean, dfmax, mism in stats:
        if f in (0, 1, 2, 5, 10, 25, 50, 100, 150, 200, 250, nframes - 1):
            print('%6d %11.6f %12.6f %13.6f %6d' % (f, dmax, dmean, dfmax, mism))

    overall = max(s[1] for s in stats)
    print('  max positional divergence over the whole run: %.6f units'
          ' (%.4f pixels at DIV=2)' % (overall, overall / 2.0))
    print('  first frame with a type/live disagreement: %s'
          % (first_discrete if first_discrete is not None else 'none'))
    if ever_outside:
        print('  !! %d samples outside the arena, first: %s'
              % (len(ever_outside), ever_outside[0]))
    else:
        print('  no object left the arena in either build')

    # is the divergence bounded or growing?
    half = nframes // 2
    early = max(s[1] for s in stats[:half]) if half else 0.0
    late = max(s[1] for s in stats[half:])
    print('  max|dpos| first half %.6f, second half %.6f -> %s'
          % (early, late, 'bounded' if late <= max(early * 4, 1.0) else 'GROWING'))

    sa, sb = stats_of(A), stats_of(B)
    print('  --- aggregate character of the two runs ---')
    print('  %-24s %12s %12s %9s' % ('', 'A', 'B', 'diff %'))
    for k in sa:
        a, b = sa[k], sb[k]
        pct = (abs(a - b) / abs(a) * 100.0) if a else 0.0
        print('  %-24s %12.4f %12.4f %8.2f%%' % (k, a, b, pct))
    print()


if __name__ == '__main__':
    main()
