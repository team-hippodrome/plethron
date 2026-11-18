"""Finds direct calls whose arguments reach the callee under the SysV x86-64 ABI (Linux, macOS) but not under the
Win64 ABI (Windows): the prototype visible at the call (or K&R promotions) disagrees with the callee's definition.

Win64 assigns argument slots by position and passes records of other sizes than 1, 2, 4 and 8 bytes by reference;
SysV assigns integer and float registers separately and passes small records in registers. So a call that passes a
4-byte color struct to a function declared with a 3-byte one works on Linux and crashes on Windows. The decomp's
func_X aliases (src/port/generated/zero_aliases.ld) hide such mismatches from the compiler; the report maps them.

usage: abi_calls.py collect <compile_commands.json> <out.json> [path substring]
       abi_calls.py report src/port/generated/zero_aliases.ld <out.json>...
Report columns: WIN64-ONLY (breaks only on Windows), BOTH (breaks everywhere), SYSV-ONLY; call site; callee; the
argument classes passed and the definition's (I<n> integer/pointer bytes, F4/F8 float, R<n> record bytes).
Needs libclang (pip install libclang).
"""
import sys, json, re, shlex, subprocess, os
RESOURCE = subprocess.run([os.environ.get('HOST_CC', 'clang'), '-print-resource-dir'], capture_output=True, text=True).stdout.strip()
def cls(tt, K):
    tt = tt.get_canonical()
    if tt.kind in (K.POINTER, K.CONSTANTARRAY, K.INCOMPLETEARRAY, K.BLOCKPOINTER): return 'I8'
    if tt.kind == K.FLOAT: return 'F4'
    if tt.kind in (K.DOUBLE, K.LONGDOUBLE): return 'F8'
    if tt.kind == K.RECORD: return 'R%d' % tt.get_size()
    if tt.kind == K.VOID: return 'V'
    s = tt.get_size()
    return 'I%d' % (s if s > 0 else 4)
def args_for(e):
    a = shlex.split(e['command'])[1:]
    out, skip = [], False
    for x in a:
        if skip: skip = False; continue
        if x in ('-o', '-MF', '-MT'): skip = True; continue
        if x in ('-c', '-MD', e['file']) or x.startswith('-g'): continue
        out.append(x)
    return out + ['-w', '-isystem', RESOURCE + '/include']
if sys.argv[1] == 'collect':
    import clang.cindex as ci
    K = ci.TypeKind
    db = json.load(open(sys.argv[2]))
    sub = sys.argv[4] if len(sys.argv) > 4 else ''
    tus = [e for e in db if e['file'].endswith('.c') and ('/game/' in e['file'] or '/src/port/' in e['file']) and sub in e['file']]
    defs, calls = [], []
    idx = ci.Index.create()
    for e in tus:
        tu = idx.parse(e['file'], args=args_for(e))
        for c in tu.cursor.walk_preorder():
            if c.kind == ci.CursorKind.FUNCTION_DECL and c.is_definition() and c.location.file and c.location.file.name == e['file']:
                t = c.type
                if t.kind == K.FUNCTIONPROTO:
                    defs.append(dict(name=c.spelling, params=[cls(a, K) for a in t.argument_types()], var=t.is_function_variadic(),
                                     file=e['file'], line=c.location.line))
            elif c.kind == ci.CursorKind.CALL_EXPR:
                ref = c.referenced
                if ref is None or ref.kind != ci.CursorKind.FUNCTION_DECL: continue
                # the callee expression must be a plain name (not a function pointer)
                kids = list(c.get_children())
                if not kids: continue
                callee = kids[0]
                while callee.kind == ci.CursorKind.UNEXPOSED_EXPR and list(callee.get_children()):
                    callee = list(callee.get_children())[0]
                if callee.kind != ci.CursorKind.DECL_REF_EXPR: continue
                t = ref.type
                args = list(c.get_arguments())
                if t.kind == K.FUNCTIONPROTO:
                    ps = [cls(a, K) for a in t.argument_types()]
                    var = t.is_function_variadic()
                    passed = ps + [cls(a.type, K) for a in args[len(ps):]] if var else ps
                    if var: passed = [p if i < len(ps) else ('F8' if p == 'F4' else p) for i, p in enumerate(passed)]
                    nfixed = len(ps) if var else None
                else:
                    passed = []
                    for a in args:
                        k = cls(a.type, K)
                        passed.append('F8' if k == 'F4' else ('I4' if k in ('I1', 'I2') else k))
                    nfixed = None
                f = c.location.file.name if c.location.file else '?'
                calls.append(dict(name=ref.spelling, passed=passed, nfixed=nfixed, noproto=t.kind != K.FUNCTIONPROTO,
                                  file=f, line=c.location.line))
    json.dump(dict(defs=defs, calls=calls), open(sys.argv[3], 'w'))
    sys.exit()

# report
aliases = {}
for line in open(sys.argv[2]):
    m = re.fullmatch(r'\s*(\w+)\s*=\s*(\w+)\s*;\s*', line)
    if m: aliases[m.group(1)] = m.group(2)
defs, calls = {}, []
for j in sys.argv[3:]:
    d = json.load(open(j))
    for x in d['defs']: defs[x['name']] = x
    calls += d['calls']
def fp(k): return k[0] == 'F'
def sysv_locs(ts, nfixed=None):
    g = x = 0; st = 0; locs = []
    for t in ts:
        if t[0] == 'R':
            n = int(t[1:])
            if n > 16: locs.append(('mem', st)); st += (n + 7) // 8; continue
            locs.append(('g', g)); g += (n + 7) // 8; continue  # INTEGER class (float-only records not modelled)
        if fp(t):
            if x < 8: locs.append(('x', x)); x += 1
            else: locs.append(('s', st)); st += 1
        else:
            if g < 6: locs.append(('g', g)); g += 1
            else: locs.append(('s', st)); st += 1
    return locs
def win_locs(ts, nfixed=None):
    locs = []
    for i, t in enumerate(ts):
        if t[0] == 'R':
            n = int(t[1:]); locs.append((('g' if i < 4 else 's') if n in (1, 2, 4, 8) else ('ref' if i < 4 else 'sref'), i)); continue
        if i >= 4: locs.append(('s', i)); continue
        if fp(t):
            # variadic args pass floats in both registers
            locs.append(('x', i) if nfixed is None or i < nfixed else ('gx', i))
        else: locs.append(('g', i))
    return locs
def ok(passed, want, locf, nfixed):
    pl = locf(passed, nfixed); wl = locf(want, None)
    have = {}
    for t, l in zip(passed, pl):
        if l[0] == 'gx': have[('g', l[1])] = t; have[('x', l[1])] = t
        else: have[l] = t
    for t, l in zip(want, wl):
        h = have.get(l)
        if h is None: return False  # garbage
        if fp(h) != fp(t): return False
        if fp(t) and h != t: return False
        if h[0] == 'R' or t[0] == 'R':
            if locf is sysv_locs:
                hs = int(h[1:]); ts = int(t[1:])
                if (hs > 8) != (ts > 8): return False
            elif h != t:
                return False
        # ints of different width: the low bits are right in both ABIs (caller-side extension aside)
    return True
seen = set(); rows = []
for c in calls:
    name = aliases.get(c['name'], c['name'])
    d = defs.get(name)
    if not d or d['var']: continue
    key = (c['file'], c['line'], c['name'])
    if key in seen: continue
    seen.add(key)
    want = d['params']
    if c['passed'] == want: continue
    s = ok(c['passed'], want, sysv_locs, c['nfixed']); w = ok(c['passed'], want, win_locs, c['nfixed'])
    if s and not w: tag = 'WIN64-ONLY'
    elif not s and not w: tag = 'BOTH'
    elif w and not s: tag = 'SYSV-ONLY'
    else: continue
    rows.append((tag, c['file'].replace('/home/user/plethron/', ''), c['line'], c['name'], name, c['passed'], want,
                 d['file'].replace('/home/user/plethron/', ''), d['line']))
for r in sorted(rows):
    print('%s\t%s:%d\t%s%s\tpassed %s\tdef %s\t(%s:%d)' % (r[0], r[1], r[2], r[3], '' if r[3] == r[4] else ' -> ' + r[4], ','.join(r[5]), ','.join(r[6]), r[7], r[8]))
