#!/usr/bin/env python3
"""Lines up guRandom call sequences: the port's PORT_GURANDOM_LOG against ares_ref's ARES_REF_PEEK of guRandom.

Usage: gurandom_diff.py <port log> <ares peek log> <port binary> <pokestadium0 symbol_addrs.txt>
ares: ARES_REF_PEEK=<log> ARES_REF_PEEK_AT="80055640@0x80079100,82100CC8" (guRandom with its seed, BattleScene_UpdateFrame).
Segments start at each reseed (seed before == 0x0a6b99cd); prints per segment the first battle frame whose calls differ."""
import sys, subprocess, bisect
START = 0x0a6b99cd
def port(path, exe):
    frames, cur, base, ras = [], None, None, set()
    for l in open(path):
        p = l.split()
        if p[0] == 'B': base = int(p[1], 16)
        elif p[0] == 'F':
            cur = []; frames.append(cur)
        elif p[0] == 'G' and cur is not None:
            cur.append((int(p[1], 16), int(p[2], 16))); ras.add(int(p[2], 16))
    # symbolize
    nm = subprocess.run(['nm', exe], capture_output=True, text=True).stdout.split('\n')
    syms = sorted((int(a, 16), n) for a, t, n in (x.split()[:3] for x in nm if len(x.split()) == 3) if t in 'tT')
    slide = base - next(a for a, n in syms if n == 'Port_XcheckGuRandomCalled')
    addrs = [a for a, _ in syms]
    name = {}
    for ra in ras:
        i = bisect.bisect_right(addrs, ra - slide) - 1
        name[ra] = syms[i][1]
    return [[(s, name[r]) for s, r in f] for f in frames]
def ares(path, symfile):
    syms = []
    for l in open(symfile):
        if '= 0x' in l:
            n, a = l.split('=')[0].strip(), l.split('=')[1].split(';')[0].strip()
            syms.append((int(a, 16), n))
    syms.sort(); addrs = [a for a, _ in syms]
    frames, cur = [], None
    for l in open(path):
        p = l.split()
        if p[2] == '82100cc8':
            cur = []; frames.append(cur)
        elif p[2] == '80055640' and cur is not None:
            ra = int(p[3][3:], 16); seed = int(p[-1].split('=')[1], 16)
            i = bisect.bisect_right(addrs, ra) - 1
            cur.append((seed, syms[i][1] if i >= 0 else hex(ra)))
    return frames
def segments(frames):
    segs = []
    for fi, f in enumerate(frames):
        for ci, (s, n) in enumerate(f):
            if s == START:
                if segs and segs[-1][0] >= fi - 3: segs[-1] = (fi, ci)  # a cluster of reseeds: keep the last
                else: segs.append((fi, ci))
    return segs
P = port(sys.argv[1], sys.argv[3]); A = ares(sys.argv[2], sys.argv[4])
sp, sa = segments(P), segments(A)
print('frames port %d ares %d, reseeds port %d ares %d' % (len(P), len(A), len(sp), len(sa)))
def flat(frames, fi, ci, n):
    out = []
    for k in range(fi, min(fi + n, len(frames))):
        f = frames[k][ci:] if k == fi else frames[k]
        out.append(f)
    return out
for k in range(min(len(sp), len(sa))):
    fp = flat(P, *sp[k], min(400, (sp[k+1][0] if k+1 < len(sp) else 10**9) - sp[k][0])); fa = flat(A, *sa[k], min(400, (sa[k+1][0] if k+1 < len(sa) else 10**9) - sa[k][0]))
    for j, (x, y) in enumerate(zip(fp, fa)):
        if [s for s, _ in x] != [s for s, _ in y]:
            print('seg %d (port frame %d, ares frame %d): differs at segment frame %d: port %d calls, ares %d' % (k, sp[k][0], sa[k][0], j, len(x), len(y)))
            from collections import Counter
            print('   port', Counter(n for _, n in x).most_common(8))
            print('   ares', Counter(n for _, n in y).most_common(8))
            break
    else:
        print('seg %d (port frame %d, ares %d): first %d frames agree (port %d, ares %d frames to the next reseed)' % (k, sp[k][0], sa[k][0], min(len(fp), len(fa)), len(fp), len(fa)))
