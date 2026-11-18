#!/usr/bin/env python3
"""Raw N64 byte offsets that reach a struct through a byte pointer, across translation units.

The decomp keeps some functions as permuter output that take `s8*`/`u8*` and read fields as
`*(u16*)(p + 0x7C2)`. Callers pass struct pointers (an ActivePokemon*, a DisplayObject*...), and the
build's -Wno-incompatible-pointer-types plus local prototypes and func_X aliases hide that. On the
N64 the offsets are right; on a 64-bit host every struct that holds a pointer before the field has a
different layout, so the access lands on another field (ActivePokemon_SetStateFlags wrote Dig's
underground flag into the Poke Ball model, and Dugtrio stayed on screen: the battle softlocked).

For every function parameter of byte-pointer type (and locals copied from it) the tool records the
raw `base + K` and `base[K]` accesses and the width read or written. Then it collects, over all TUs, the struct
types actually passed for that parameter (direct calls, through aliases; casts at the call site
are looked through). Each (struct, K) is mapped to the field at N64 (ILP32, big-endian) offset K
and that field's host offset:

  MISMATCH  the field sits at another host offset: use the field.
  WIDTH     same offset, but the access is narrower or wider than the field (wrong byte on a
            little-endian host, e.g. a u8 read of a u16 flags word).
  NOCALLER  the function is only reached through a pointer (callback table), or only gets byte
            pointers: the struct is unknown, review by hand (listed so the class can't hide).
  NARROW    bytes or halfwords of a lifted word read in place, e.g. M2C_FIELD(&D_88002254, u8*, 0):
            the lifter keeps data declared s32/M2C_UNK in host order, so on a little-endian host
            byte 0 is the N64's byte 3 (the main menu's grey background came out pink). Use
            Port_WordByte / Port_WordsToBytes (port_endian.h). `report --all` also lists narrow
            views of objects that are not lifted words (BSS buffers, byte-typed data: usually fine).

Usage (from the plethron root):
  python3 tools/raw_offsets.py collect out.json game/src/**/*.c      # once per chunk, -j via xargs
  python3 tools/raw_offsets.py report [--all] out*.json
Needs `pip install libclang`. Flags come from tools/compile_flags.txt.
"""
import json, pathlib, re, sys
import clang.cindex as ci

K = ci.TypeKind
CK = ci.CursorKind
ROOT = pathlib.Path(__file__).resolve().parent.parent
BYTE = {'s8', 'u8', 'char', 'signed char', 'unsigned char', 'void'}


def flags():
    out = []
    for line in (ROOT / 'tools' / 'compile_flags.txt').read_text().splitlines():
        line = line.strip()
        if not line or line.startswith('-W') or line == '-w':
            continue
        if line.startswith('-I') and not line.startswith('-I/'):
            line = '-I' + str(ROOT / line[2:])
        out.append(line)
    inc = sorted(pathlib.Path('/usr/lib').glob('llvm-*/lib/clang/*/include'))
    if inc:
        out += ['-isystem', str(inc[-1])]
    return out + ['-w', '-include', str(ROOT / 'src/port/sdk/port_compat.h')]


def strip(c):
    while c.kind in (CK.PAREN_EXPR, CK.UNEXPOSED_EXPR, CK.CSTYLE_CAST_EXPR):
        ch = [x for x in c.get_children() if x.kind != CK.TYPE_REF]
        if len(ch) != 1:
            break
        c = ch[-1]
    return c


def lit(c):
    c = strip(c)
    if c.kind == CK.INTEGER_LITERAL:
        t = [x.spelling for x in c.get_tokens()]
        try:
            return int(t[0].rstrip('uUlL'), 0) if t else None
        except ValueError:
            return None
    return None


def is_byteptr(t):
    t = t.get_canonical()
    return t.kind == K.POINTER and t.get_pointee().get_canonical().spelling.replace('const ', '').replace('volatile ', '') in BYTE


# ---------------------------------------------------------------- N64 layout (ILP32, from offs.py)
n64cache = {}


def n64(t):
    t = t.get_canonical()
    k = t.kind
    if k == K.POINTER:
        return 4, 4
    if k == K.CONSTANTARRAY:
        s, a = n64(t.element_type)
        return s * t.element_count, a
    if k == K.RECORD:
        key = t.spelling
        if key in n64cache:
            return n64cache[key][:2]
        union = t.get_declaration().kind == CK.UNION_DECL
        bit = 0  # MIPS/IDO packing: a bitfield shares its type's storage unit unless it would straddle it
        al = 1
        fields = []
        for f in t.get_fields():
            fs, fa = n64(f.type)
            if union:
                fo = 0
                bit = max(bit, fs * 8)
            elif f.is_bitfield():
                bw = f.get_bitfield_width()
                unit = fs * 8
                if bw == 0:
                    bit = (bit + unit - 1) // unit * unit
                    continue
                if bit % unit + bw > unit:
                    bit = (bit + unit - 1) // unit * unit
                fo = bit // 8 // fs * fs
                bit += bw
            else:
                bit = (bit + fa * 8 - 1) // (fa * 8) * (fa * 8)
                fo = bit // 8
                bit += fs * 8
            al = max(al, fa)
            fields.append((f, fo, fs))
        off = (bit + 7) // 8
        size = (off + al - 1) // al * al
        n64cache[key] = (size, al, fields)
        return size, al
    if k == K.ENUM:
        return 4, 4
    if k in (K.LONG, K.ULONG):
        return 4, 4
    sz = t.get_size()
    return (sz, sz) if sz > 0 else (0, 1)


def path_at(t, off):
    """(field path, host offset, field size) of N64 offset `off` inside t."""
    t = t.get_canonical()
    if t.kind == K.CONSTANTARRAY:
        es, _ = n64(t.element_type)
        if es == 0:
            return '', None, 0
        i = off // es
        p, h, s = path_at(t.element_type, off - i * es)
        return '[%d]' % i + p, None if h is None else i * t.element_type.get_canonical().get_size() + h, s
    if t.kind == K.RECORD:
        n64(t)
        for f, fo, fs in n64cache[t.spelling][2]:
            if fo <= off < fo + max(fs, 1):
                ho = t.get_offset(f.spelling) // 8 if f.spelling else 0
                p, h, s = path_at(f.type, off - fo)
                return '.' + f.spelling + p, None if h is None else ho + h, s
        return '?+%#x' % off, None, 0
    sz = n64(t)[0]
    return ('' if off == 0 else '+%d' % off), off, sz


# ---------------------------------------------------------------- collect
def loc(c):
    f = c.location.file
    return '%s:%d' % (pathlib.Path(f.name).relative_to(ROOT) if f and f.name.startswith(str(ROOT)) else (f.name if f else '?'), c.location.line)


def collect(out_path, files):
    idx = ci.Index.create()
    args = flags()
    accesses = []   # func, param index, offset, width, loc
    narrow = []     # loc, source: bytes/halves of a wider scalar read in place (host order is little-endian)
    calls = []      # callee, arg index, record spelling, loc
    records = {}    # spelling -> {offset: [path, host, size]} filled at report time from a TU
    for path in files:
        path = str(pathlib.Path(path).resolve())
        tu = idx.parse(path, args=args)

        def walk_fn(fn):
            params = [p for p in fn.get_children() if p.kind == CK.PARM_DECL]
            taint = {p.spelling: i for i, p in enumerate(params) if is_byteptr(p.type)}
            if not taint:
                return

            def visit(c, parent_chain):
                # locals copied from a tainted parameter: s8* x = (s8*)param; x = param;
                if c.kind == CK.VAR_DECL and is_byteptr(c.type):
                    ch = list(c.get_children())
                    if ch:
                        r = strip(ch[-1])
                        if r.kind == CK.DECL_REF_EXPR and r.spelling in taint:
                            taint[c.spelling] = taint[r.spelling]
                if c.kind == CK.BINARY_OPERATOR:
                    kids = list(c.get_children())
                    toks = [t.spelling for t in c.get_tokens()]
                    if len(kids) == 2:
                        L, R = strip(kids[0]), kids[1]
                        if '=' in toks and L.kind == CK.DECL_REF_EXPR and is_byteptr(L.type):
                            r = strip(R)
                            if r.kind == CK.DECL_REF_EXPR and r.spelling in taint:
                                taint[L.spelling] = taint[r.spelling]
                        v = lit(R)
                        if v is not None and L.kind == CK.DECL_REF_EXPR and L.spelling in taint \
                                and is_byteptr(L.type) and '+' in toks:
                            width = 1
                            # parent chain: (cast to T*) then deref -> width of T
                            for p in reversed(parent_chain[-4:]):
                                pt = p.type.get_canonical()
                                if p.kind == CK.CSTYLE_CAST_EXPR and pt.kind == K.POINTER:
                                    width = max(pt.get_pointee().get_canonical().get_size(), 1)
                                    break
                            accesses.append((fn.spelling, taint[L.spelling], v, width, loc(c)))
                if c.kind == CK.ARRAY_SUBSCRIPT_EXPR:
                    # p[K] on a tainted byte pointer: the same raw offset as *(p + K)
                    kids = list(c.get_children())
                    if len(kids) == 2:
                        L = strip(kids[0])
                        v = lit(kids[1])
                        if v is not None and L.kind == CK.DECL_REF_EXPR and L.spelling in taint and is_byteptr(L.type):
                            accesses.append((fn.spelling, taint[L.spelling], v, 1, loc(c)))
                for ch in c.get_children():
                    visit(ch, parent_chain + [c])

            visit(fn, [])

        def narrow_check(ch):
            # (u8*)&word, (u8*)words, M2C_FIELD(&word, u8*, k): a narrower view of a wider integer or float object
            if ch.kind != CK.CSTYLE_CAST_EXPR:
                return
            ct = ch.type.get_canonical()
            if ct.kind != K.POINTER:
                return
            nt = ct.get_pointee().get_canonical()
            nsz = nt.get_size()
            if nt.kind not in (K.CHAR_S, K.SCHAR, K.UCHAR, K.CHAR_U, K.SHORT, K.USHORT):
                return
            inner = [x for x in ch.get_children() if x.kind != CK.TYPE_REF]
            if not inner:
                return
            it = strip(inner[-1]).type.get_canonical()
            if it.kind == K.POINTER:
                wt = it.get_pointee().get_canonical()
            elif it.kind in (K.CONSTANTARRAY, K.INCOMPLETEARRAY):
                wt = it.element_type.get_canonical()
            else:
                return
            if wt.kind in (K.INT, K.UINT, K.LONG, K.ULONG, K.FLOAT, K.SHORT, K.USHORT) and wt.get_size() > nsz:
                narrow.append((loc(ch), wt.spelling, nt.spelling, ' '.join(t.spelling for t in ch.get_tokens())[:120]))

        def walk(c):
            for ch in c.get_children():
                if ch.location.file and ch.location.file.name == path:
                    narrow_check(ch)
                if ch.location.file and ch.location.file.name != path and ch.kind == CK.FUNCTION_DECL:
                    continue
                if ch.kind == CK.FUNCTION_DECL and ch.is_definition():
                    walk_fn(ch)
                if ch.kind == CK.CALL_EXPR and ch.referenced is not None:
                    for i, a in enumerate(ch.get_arguments()):
                        at = strip(a).type.get_canonical()
                        if at.kind == K.POINTER and at.get_pointee().get_canonical().kind == K.RECORD:
                            rt = at.get_pointee().get_canonical()
                            calls.append((ch.referenced.spelling, i, rt.spelling, loc(ch)))
                            if rt.spelling not in records:
                                records[rt.spelling] = rt
                walk(ch)

        walk(tu.cursor)
        # resolve the offsets this TU can see right away (types live only as long as the TU)
    # layout table for every record seen as an argument, at every offset any function used
    offs = sorted({(a[2], a[3]) for a in accesses})
    lay = {}
    for name, rt in records.items():
        try:
            size = n64(rt)[0]
        except Exception:
            continue
        lay[name] = {'size': size, 'at': {}}
        for v, w in offs:
            if v < size:
                p, h, s = path_at(rt, v)
                lay[name]['at']['%d' % v] = [p, h, s]
    json.dump({'accesses': accesses, 'calls': calls, 'layouts': lay, 'narrow': narrow}, open(out_path, 'w'))


# ---------------------------------------------------------------- report
def aliases():
    m = {}
    p = ROOT / 'src/port/generated/zero_aliases.ld'
    if p.exists():
        for line in p.read_text().splitlines():
            r = re.match(r'\s*(\w+)\s*=\s*(\w+)\s*;', line)
            if r:
                m[r.group(1)] = r.group(2)
    return m


def lifted_words():
    """Lifted symbols whose storage is a host-order 2/4/8-byte scalar (word-swapped from the ROM image)."""
    p = ROOT / 'src/port/generated/zero_lift_data.c'
    if not p.exists():
        return {}
    text = p.read_text()
    types = []
    body = text[text.index('gZLiftTypes[]'):]
    for line in body.splitlines()[1:]:
        if line.startswith('}'):
            break
        r = re.search(r'//\s*(\S+)', line)
        types.append(r.group(1) if r else '')
    out = {}
    for m in re.finditer(r'\{ 0x[0-9A-F]+, 0x[0-9A-F]+, \d+, (\d+), \d+, \d+, 0x[0-9A-F]+, (\w+) \}', text):
        t = int(m.group(1))
        name = types[t] if t < len(types) else ''
        if re.fullmatch(r'(a?s[248][su]|f[48])(\[\d+\])?', name):
            out[m.group(2)] = name
    return out


def report(paths, show_all=False):
    al = aliases()
    words = lifted_words()
    acc, calls, lay, nar = [], [], {}, []
    for p in paths:
        d = json.load(open(p))
        acc += d['accesses']
        nar += [tuple(x) for x in d.get('narrow', [])]
        calls += d['calls']
        for k, v in d['layouts'].items():
            lay.setdefault(k, {'size': v['size'], 'at': {}})['at'].update(v['at'])
    passed = {}
    for callee, i, rec, where in calls:
        callee = al.get(callee, callee)
        passed.setdefault((callee, i), {}).setdefault(rec, where)
    seen = set()
    rows = []
    for fn, pi, v, w, where in acc:
        key = (fn, pi, v, w, where)
        if key in seen:
            continue
        seen.add(key)
        recs = passed.get((fn, pi), {})
        if not recs:
            rows.append(('NOCALLER', where, fn, pi, hex(v), w, '', '', ''))
            continue
        for rec, cw in sorted(recs.items()):
            L = lay.get(rec)
            if not L or str(v) not in L['at']:
                continue
            fp, host, fsz = L['at'][str(v)]
            if host != v:
                rows.append(('MISMATCH', where, fn, pi, hex(v), w, rec, fp, 'host %s (caller %s)' % (hex(host) if host is not None else '?', cw)))
            elif fsz and w != fsz and fsz in (2, 4, 8) and w in (1, 2, 4):
                rows.append(('WIDTH', where, fn, pi, hex(v), w, rec, fp, 'field is %d bytes (caller %s)' % (fsz, cw)))
    for n in sorted(set(nar)):
        lifted = [w for w in re.findall(r'\w+', n[3]) if w in words]
        if lifted:
            rows.append(('NARROW', n[0], '', '', '', '', n[1] + ' as ' + n[2], lifted[0] + ' lifted as ' + words[lifted[0]], n[3]))
        elif show_all:
            rows.append(('narrow?', n[0], '', '', '', '', n[1] + ' as ' + n[2], '', n[3]))
    for r in sorted(rows, key=lambda r: (r[0], r[1])):
        print('\t'.join(str(x) for x in r))


if __name__ == '__main__':
    if sys.argv[1] == 'collect':
        collect(sys.argv[2], sys.argv[3:])
    else:
        args = [a for a in sys.argv[2:] if a != '--all']
        report(args, show_all='--all' in sys.argv)
