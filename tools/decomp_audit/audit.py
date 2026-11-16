#!/usr/bin/env python3 -I
"""Lists what the port's copy of the game code lost or changed relative to the decomp it was taken from.

Usage: tools/decomp_audit/audit.py DECOMP_SRC [--symbols symbol_addrs.txt ...] [--port game/src] [-o report.txt]

DECOMP_SRC is the decomp's src/ at the commit in game/UPSTREAM.md, e.g.
    git -C ../pokestadium0 archive 35d7a61 src | tar -x -C /tmp/decomp
    tools/decomp_audit/audit.py /tmp/decomp/src --symbols ../pokestadium0/linker_scripts/jp/symbol_addrs.txt

Sections of the report (one line per function; review each by hand, most are legitimate port changes):
  missing      functions or files of the decomp the port does not have
  structure    functions with fewer statements, assignments, control keywords or calls than the decomp
  returns      functions with more `return`s than the decomp (early outs the port added)
  constants    statements that differ only in a numeric literal (NULL/0 and hex floats are filtered out)
  globals      decomp globals a function no longer mentions at all (views re-indexed through another symbol)
"""
import argparse, collections, difflib, os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cdefs as d  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def stmts(body, norm):
    out, cur = [], []
    for t in body:
        cur.append(norm(t))
        if t in (';', '{', '}'):
            out.append(tuple(cur))
            cur = []
    if cur:
        out.append(tuple(cur))
    return out


def same_number(a, b):
    if {a, b} <= {'NULL', 'N0'}:
        return True
    if a.startswith('N') and b.startswith('F'):  # m2c's float bits (0x3F800000) became a float literal
        try:
            f = struct.unpack('>f', struct.pack('>I', int(a[1:])))[0]
            return abs(f - float(b[1:])) <= 1e-5 * max(1.0, abs(f))
        except (ValueError, struct.error):
            return False
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('decomp_src')
    ap.add_argument('--port', default=os.path.join(ROOT, 'game', 'src'))
    ap.add_argument('--symbols', nargs='*', default=[])
    ap.add_argument('-o', '--output')
    a = ap.parse_args()
    ren = d.load_renames(a.symbols)

    def norm(t):
        t = ren.get(t, t)
        m = re.fullmatch(r'(?:func|D|B|jtbl)_([0-9A-Fa-f]{8})', t)
        if m:
            return '@' + m.group(1).upper()
        if re.match(r'\d', t) or (t.startswith('.') and len(t) > 1):
            return d.num(t)
        return t

    sec = collections.OrderedDict((k, []) for k in ('missing', 'structure', 'returns', 'constants', 'globals'))
    differing = 0
    for rel, db, dp in d.walk_pairs(a.decomp_src, a.port):
        if dp is None:
            sec['missing'].append(f'{rel}: file')
            continue
        for n, (_, body) in db.items():
            if n not in dp:
                sec['missing'].append(f'{rel}: {n}')
                continue
            pbody = dp[n][1]
            fb, fp = d.feats(body, ren), d.feats(pbody, ren)
            if fb != fp:
                differing += 1
            miss, extra = fb - fp, fp - fb
            st = {k: v for k, v in miss.items() if k in ('stmt', 'asg') or k.startswith(('kw:', 'call:'))}
            if st:
                sec['structure'].append(f'{rel}: {n}: ' + ', '.join(f'{k} -{v}' for k, v in sorted(st.items())))
            if extra.get('kw:return'):
                sec['returns'].append(f'{rel}: {n}: +{extra["kw:return"]} return')
            gone = [k[3:] for k in miss if k.startswith('id:') and re.match(r'@|g[A-Z]|s[A-Z]|D_', k[3:])
                    and not fp.get(k) and not fp.get('call:' + k[3:])]
            if gone:
                sec['globals'].append(f'{rel}: {n}: ' + ' '.join(gone))
            sb, sp = stmts(body, norm), stmts(pbody, norm)
            if sb != sp:
                sm = difflib.SequenceMatcher(None, sb, sp, autojunk=False)
                for op, i1, i2, j1, j2 in sm.get_opcodes():
                    if op != 'replace':
                        continue
                    for x, y in zip(sb[i1:i2], sp[j1:j2]):
                        if len(x) != len(y):
                            continue
                        diffs = [(p, q) for p, q in zip(x, y) if p != q]
                        if diffs and all(re.match(r'[NF]', p) and re.match(r'[NF]', q) for p, q in diffs) \
                                and not all(same_number(p, q) or same_number(q, p) for p, q in diffs):
                            sec['constants'].append(f'{rel}: {n}: {diffs} in `{" ".join(x)[:120]}`')
    out = [f'# decomp audit: {differing} functions differ from the decomp\n']
    for k, rows in sec.items():
        out.append(f'\n## {k} ({len(rows)})\n')
        out += [r + '\n' for r in rows]
    text = ''.join(out)
    if a.output:
        open(a.output, 'w').write(text)
    else:
        sys.stdout.write(text)


if __name__ == '__main__':
    main()
