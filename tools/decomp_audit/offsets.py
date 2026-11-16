#!/usr/bin/env python3 -I
"""Checks the port's offset-to-field rewrites against the real N64 struct layout.

Usage: tools/decomp_audit/offsets.py DECOMP_SRC [--port game/src]

The decomp reads many struct fields through raw byte offsets, M2C_FIELD(p, T*, 0x1C) (also nested:
M2C_FIELD(M2C_FIELD(p, T*, 0x144), T*, 0x1E)). The port rewrote them as fields (p->mon.hp, p->voice->env_phase), and
a wrong field there reads the wrong memory without any warning. For every such pair in aligned statements, this
asserts on a 32-bit big-endian MIPS target (where clang lays structs out exactly as on the N64) that each field path
starts at the decomp's offset, hop by hop: offsetof(T0, a) == K0, offsetof(typeof(*T0.a), b) == K1, ...

It reports the pairs whose offsets disagree (FAIL) and the ones it could not compile (ERR: usually a variable whose
type it guessed wrong). A FAIL is either a real bug or a pairing mistake (two rewrites in one statement swapped): read
the function. Pairs are only taken when both sides have the same number of accesses on the same root.
"""
import argparse, collections, concurrent.futures, difflib, json, os, re, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cdefs as d  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def stmts(body):
    out, cur = [], []
    for t in body:
        cur.append(t)
        if t in (';', '{', '}'):
            out.append(cur)
            cur = []
    if cur:
        out.append(cur)
    return out


ID=re.compile(r'[A-Za-z_]\w*$')
def stmts(body):
    out=[];cur=[]
    for t in body:
        cur.append(t)
        if t in (';','{','}'): out.append(cur); cur=[]
    if cur: out.append(cur)
    return out
def parse_m2c(t,i):
    """t[i]=='M2C_FIELD' -> (root, amp, [K...], end) or None"""
    if t[i]!='M2C_FIELD' or t[i+1]!='(': return None
    j=i+2; amp=False
    if t[j]=='M2C_FIELD':
        r=parse_m2c(t,j)
        if not r: return None
        root,amp,ks,j=r
    else:
        if t[j]=='&': amp=True; j+=1
        if t[j]=='(' : return None
        if not ID.match(t[j]): return None
        root=t[j]; ks=[]; j+=1
    if t[j]!=',': return None
    depth=0; j+=1
    while j<len(t) and not (t[j]==',' and depth==0):
        if t[j]=='(': depth+=1
        elif t[j]==')': depth-=1
        j+=1
    if j+2>=len(t) or t[j+2]!=')' or not re.match(r'(0x)?[0-9]',t[j+1]): return None
    return root,amp,ks+[int(t[j+1].rstrip('uUlL'),0)],j+3
def m2cs(t):
    res=[];i=0
    while i<len(t):
        if t[i]=='M2C_FIELD':
            r=parse_m2c(t,i)
            if r: res.append(r[:3]); i=r[3]; continue
        i+=1
    return res
def chains(t):
    res=[]
    for i,x in enumerate(t):
        if ID.match(x) and i+2<len(t) and t[i+1] in ('->','.') and (i==0 or t[i-1] not in ('->','.')) and (i==0 or t[i-1]!='&' or True):
            hops=[]; first=t[i+1]; cur=[t[i+2]]; j=i+3; ok=True
            while j<len(t):
                if t[j]=='.' and j+1<len(t): cur.append('.'+t[j+1]); j+=2
                elif t[j]=='[':
                    if j+2<len(t) and re.match(r'(0x)?\d+$',t[j+1]) and t[j+2]==']': cur.append('['+t[j+1]+']'); j+=3
                    else: ok=False; break
                elif t[j]=='->' and j+1<len(t): hops.append(''.join(cur)); cur=[t[j+1]]; j+=2
                else: break
            if ok: hops.append(''.join(cur)); res.append((x,first,hops))
    return res
def vartype(src,var):
    m=re.findall(r'\b((?:struct\s+)?[A-Za-z_]\w*)\s*\*\s*'+re.escape(var)+r'\s*[;,)=]',src)
    m=[x for x in m if x not in ('return','sizeof')]
    return m[-1] if m else None


def func_source(psrc, name):
    m = re.search(r'\b' + re.escape(name) + r'\s*\([^;{]*\)\s*\{', psrc)
    if not m:
        return ''
    i, dd = m.end(), 1
    while dd and i < len(psrc):
        if psrc[i] == '{':
            dd += 1
        elif psrc[i] == '}':
            dd -= 1
        i += 1
    return psrc[m.start():i]


def collect(base, port):
    checks = collections.defaultdict(list)
    for rel, db, dp in d.walk_pairs(base, port):
        if dp is None:
            continue
        psrc = open(os.path.join(port, rel), errors='replace').read()
        for n, (_, b) in db.items():
            if n not in dp:
                continue
            sb, sp = stmts(b), stmts(dp[n][1])
            fsrc = func_source(psrc, n)
            sm = difflib.SequenceMatcher(None, [tuple(s) for s in sb], [tuple(s) for s in sp], autojunk=False)
            for op, i1, i2, j1, j2 in sm.get_opcodes():
                if op != 'replace':
                    continue
                A = [t for s in sb[i1:i2] for t in s]
                B = [t for s in sp[j1:j2] for t in s]
                byv = collections.defaultdict(list)
                for r, amp, ks in m2cs(A):
                    byv[(r, amp)].append(ks)
                byc = collections.defaultdict(list)
                for r, first, hops in chains(B):
                    byc[(r, first == '.')].append(hops)
                for (r, amp), lst in byv.items():
                    cl = byc.get((r, amp), [])
                    if len(lst) != len(cl):
                        continue
                    if amp:
                        t0 = f'__typeof__({r})'
                    else:
                        t0 = vartype(fsrc, r) or f'__typeof__(*{r})'
                    for ks, hops in zip(lst, cl):
                        if len(ks) == len(hops):
                            checks[rel].append((n, r, t0, hops, ks))
    return checks


def run(rel, items, port_dir):
    flags = [l.strip() for l in open(os.path.join(ROOT, 'tools', 'compile_flags.txt')) if l.strip()]
    flags = [f for f in flags if f != '-w'] + ['-w', '-ferror-limit=0']
    shim = os.path.join(ROOT, 'build', 'decomp_audit', 'sysroot')
    os.makedirs(os.path.join(shim, 'gnu'), exist_ok=True)
    open(os.path.join(shim, 'gnu', 'stubs-32.h'), 'a').close()  # glibc's headers ask for it on a 32-bit target
    src = os.path.join(port_dir, rel)
    tmp = src[:-2] + '__offsets_check.c'
    body = open(src, errors='replace').read()
    lines = ['\n']
    for i, (fn, r, t0, hops, ks) in enumerate(items):
        T = t0
        for h, (path, K) in enumerate(zip(hops, ks)):
            lines.append(f'_Static_assert(__builtin_offsetof({T}, {path}) == {K}, "OFFCHK {i} {h}");\n')
            T = f'__typeof__(*((({T}*)0)->{path}))'
    open(tmp, 'w').write(body + ''.join(lines))
    try:
        r = subprocess.run(['clang', '-fsyntax-only', '--target=mips-unknown-linux-gnu', '-idirafter',
                            'tools/mips_sysroot', '-idirafter', shim, '-idirafter', '/usr/include/x86_64-linux-gnu']
                           + flags + [tmp], cwd=ROOT, capture_output=True, text=True, errors='replace')
    finally:
        os.remove(tmp)
    n = body.count('\n')
    fails, errs = set(), []
    for l in r.stderr.split('\n'):
        m = re.search(r'OFFCHK (\d+) (\d+)', l)
        if m:
            fails.add((int(m.group(1)), int(m.group(2))))
            continue
        m = re.search(r'__offsets_check\.c:(\d+):\d+: error: (.*)', l)
        if m and int(m.group(1)) > n:
            errs.append(m.group(2))
    return rel, fails, errs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('decomp_src')
    ap.add_argument('--port', default=os.path.join(ROOT, 'game', 'src'))
    ap.add_argument('-j', type=int, default=os.cpu_count() or 4)
    a = ap.parse_args()
    checks = collect(a.decomp_src, a.port)
    total = sum(len(v) for v in checks.values())
    nfail = nerr = 0
    with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
        for rel, fails, errs in ex.map(lambda k: run(k, checks[k], a.port), list(checks)):
            for i, h in sorted(fails):
                fn, r, t0, hops, ks = checks[rel][i]
                print(f'FAIL {rel}: {fn}: {r}->{"->".join(hops)} hop {h}: decomp offsets {[hex(k) for k in ks]} ({t0})')
                nfail += 1
            for e in errs:
                print(f'ERR  {rel}: {e[:160]}')
                nerr += 1
    print(f'{total} rewritten accesses checked in {len(checks)} files: {nfail} offset mismatches, {nerr} not compiled')


if __name__ == '__main__':
    main()
