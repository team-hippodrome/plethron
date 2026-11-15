#!/usr/bin/env python3
"""Generate the data lifter tables (src/port/generated/zero_lift_*.{c,S}).

Stadium Zero's decomp keeps most data, rodata and bss in assembly: the C code only declares the symbols
(`extern s32 D_8006F3E0[];`) and the linker scripts PROVIDE their addresses. The host build has no such data, so the
port defines storage for every one of these symbols here and the runtime (src/port/zero_lift.c) fills it from the
user's ROM: byte order swapped, pointers widened and relocated to host addresses, function pointers mapped to the
statically linked functions. Nothing ROM-derived is generated: only names, addresses and type layouts.

Inputs:
  syms.json   per-image symbol map of the decomp build (tools/zero/read_maps.py)
  decls.json  types of the extern declarations and definitions (tools/zero/collect_decls.py, libclang)
  nm.txt      `nm` of libstadium_game.a (which names are host functions / defined data)
  undef.txt   names the game objects reference but do not define

Type rules: the N64 layout is ILP32 big-endian (pointer = 4 bytes, MIPS o32 alignment and bitfield rules); the host
layout is what clang computed. Unions lift their pointer member when one overlaps a 32-bit scalar (MAYBEPTR: the
runtime relocates the word only if it is an address it knows), else their largest member.
"""
import json, sys, bisect, collections, re

K_SCALAR, K_PTR, K_FNPTR, K_MAYBEPTR, K_REC, K_BITS, K_RAW = range(7)


def align_up(x, a):
    return (x + a - 1) // a * a


class Gen:
    def __init__(s, types):
        s.rtypes = types          # record id -> clang record description
        s.ids = {}                # type key -> index
        s.tab = []                # [ (key, n64size, hostsize, n64align, ops) ]
        s.n64cache = {}
        s.warn = collections.Counter()

    # ---- layouts -------------------------------------------------------------------------------------------------
    def key(s, t):
        k = t['k']
        if k == 's': return 's%d%s' % (t['size'], 's' if t['sg'] else 'u')
        if k == 'f': return 'f%d' % t['size']
        if k == 'ptr': return 'p'
        if k == 'fnptr': return 'fp'
        if k == 'iptr': return 'ip'
        if k == 'rec': return 'r:' + t['id']
        if k == 'array': return 'a%s[%s]' % (s.key(t['of']), t['n'])
        return k + ':' + t.get('name', '')

    def n64(s, t):
        """(size, align) on the N64."""
        k = t['k']
        if k in ('s', 'f'): return t['size'], t['size']
        if k in ('ptr', 'fnptr', 'iptr'): return 4, 4
        if k == 'array':
            sz, al = s.n64(t['of'])
            return sz * (t['n'] or 0), al
        if k == 'rec':
            return s.n64rec(t['id'])[:2]
        return 0, 1

    def host(s, t):
        k = t['k']
        if k in ('s', 'f'): return t['size'], t['size']
        if k in ('ptr', 'fnptr', 'iptr'): return 8, 8
        if k == 'array':
            sz, al = s.host(t['of'])
            return sz * (t['n'] or 0), al
        if k == 'rec':
            r = s.rtypes[t['id']]
            return r['size'], r['align']
        return 0, 1

    def n64rec(s, rid):
        """(size, align, [n64 bit offset per field]) of a record on the N64."""
        if rid in s.n64cache: return s.n64cache[rid]
        r = s.rtypes[rid]
        offs = []
        if r['k'] == 'union':
            size = al = 1
            for f in r['fields']:
                fs, fa = s.n64(f['t'])
                offs.append(0); size = max(size, fs); al = max(al, fa)
            res = (align_up(size, al), al, offs)
        else:
            bit = 0; al = 1
            for f in r['fields']:
                fs, fa = s.n64(f['t'])
                al = max(al, fa)
                if 'w' in f:
                    unit = fs * 8
                    if f['w'] == 0:
                        bit = align_up(bit, unit)
                    elif bit // unit != (bit + f['w'] - 1) // unit:
                        bit = align_up(bit, unit)
                    offs.append(bit); bit += f['w']
                else:
                    bit = align_up(bit, fa * 8)
                    offs.append(bit); bit += fs * 8
            res = (align_up(align_up(bit, 8) // 8, al), al, offs)
        s.n64cache[rid] = res
        return res

    def has_ptr(s, t, seen=()):
        k = t['k']
        if k in ('ptr', 'fnptr', 'iptr'): return True
        if k == 'array': return s.has_ptr(t['of'], seen)
        if k == 'rec':
            if t['id'] in seen: return False
            return any(s.has_ptr(f['t'], seen + (t['id'],)) for f in s.rtypes[t['id']]['fields'])
        return False

    # ---- type table ----------------------------------------------------------------------------------------------
    def tid(s, t):
        """Index of a type in the table (ops filled lazily so recursive pointers terminate)."""
        if t is None or t['k'] in ('void', 'opaque', 'function'): return -1
        if t['k'] == 'array' and not t['n']: t = t['of']
        key = s.key(t)
        if key in s.ids: return s.ids[key]
        i = len(s.tab)
        s.ids[key] = i
        s.tab.append(None)
        n64s, n64a = s.n64(t)
        hs, ha = s.host(t)
        ops = []
        s.emit(t, 0, 0, ops)
        s.tab[i] = (key, n64s, hs, n64a, s.coalesce(ops))
        return i

    def emit(s, t, n, h, ops, count=1, ns=0, hs=0):
        """Append the ops of type t placed at n64 offset n / host offset h, repeated count times."""
        k = t['k']
        if k in ('s', 'f'):
            ops.append([K_SCALAR, n, h, t['size'], count, ns or t['size'], hs or t['size'], -1, 0, 0, 0, 0])
        elif k == 'ptr':
            ops.append([K_PTR, n, h, 4, count, ns or 4, hs or 8, s.tid(t['to']) if t['to'] else -1, 0, 0, 0, 0])
        elif k == 'iptr':  # an address-sized integer: relocated when it holds a link address
            ops.append([K_MAYBEPTR, n, h, 4, count, ns or 4, hs or 8, -1, 0, 0, 0, 0])
        elif k == 'fnptr':
            ops.append([K_FNPTR, n, h, 4, count, ns or 4, hs or 8, -1, 0, 0, 0, 0])
        elif k == 'array':
            if not t['n']: return
            en, _ = s.n64(t['of']); eh, _ = s.host(t['of'])
            if count == 1:
                s.emit(t['of'], n, h, ops, t['n'], en, eh)
            else:
                ops.append([K_REC, n, h, 0, count, ns, hs, s.tid(t), 0, 0, 0, 0])
        elif k == 'rec':
            if count != 1:
                ops.append([K_REC, n, h, 0, count, ns, hs, s.tid(t), 0, 0, 0, 0])
                return
            r = s.rtypes[t['id']]
            _, _, noffs = s.n64rec(t['id'])
            if r['k'] == 'union':
                f = s.pick_union(r)
                if f == 'maybeptr':
                    ops.append([K_MAYBEPTR, n, h, 4, 1, 4, 8, -1, 0, 0, 0, 0])
                elif f is not None:
                    s.emit(f['t'], n, h, ops)
                return
            for f, nb in zip(r['fields'], noffs):
                if 'w' in f:
                    if f['w'] == 0: continue
                    unit = s.n64(f['t'])[0]
                    sg = 1 if f['t'].get('sg') else 0
                    ops.append([K_BITS, n + nb // 8 // unit * unit, h + f['bit'] // 8 // unit * unit, unit, 1, unit,
                                unit, -1, nb % (unit * 8), f['bit'] % (unit * 8), f['w'], sg])
                else:
                    s.emit(f['t'], n + nb // 8, h + f['bit'] // 8, ops)
        else:
            s.warn['field of kind ' + k] += 1

    def pick_union(s, r):
        fs = r['fields']
        if not fs: return None
        if any(f['t']['k'] in ('ptr', 'fnptr') for f in fs) and any(
                f['t']['k'] == 's' and f['t']['size'] == 4 for f in fs):
            return 'maybeptr'
        best = None; bs = -1
        for f in fs:
            sz = s.n64(f['t'])[0] + (1 if s.has_ptr(f['t']) else 0)
            if sz > bs: best, bs = f, sz
        return best

    @staticmethod
    def coalesce(ops):
        out = []
        for o in ops:
            p = out[-1] if out else None
            if (p and p[0] == K_SCALAR and o[0] == K_SCALAR and p[3] == o[3] and o[4] == 1 and
                    p[5] == p[3] and p[6] == p[3] and o[1] == p[1] + p[4] * p[3] and o[2] == p[2] + p[4] * p[3]):
                p[4] += 1
                continue
            out.append(list(o))
        return out

    # ---- offset translation (generation time, for aliases into the middle of a symbol) ---------------------------
    def translate(s, ti, off):
        key, n64s, hs, _, ops = s.tab[ti]
        if n64s and off >= n64s:
            return (off // n64s) * hs + s.translate(ti, off % n64s)
        for o in ops:
            kind, n, h, size, count, ns, hst, sub = o[:8]
            if kind == K_REC:
                ext = ns * count
                if n <= off < n + ext:
                    i = (off - n) // ns
                    return h + i * hst + s.translate(sub, (off - n) % ns)
            else:
                ext = ns * (count - 1) + size
                if n <= off < n + ext:
                    i = (off - n) // ns; r = (off - n) % ns
                    if r >= size: return None
                    if kind == K_SCALAR: return h + i * hst + (size - 1 - r if r else 0)
                    if kind in (K_PTR, K_FNPTR, K_MAYBEPTR): return h + i * hst + (r if r == 0 else 0)
                    return h + i * hst + r
        return None


def main():
    syms_p, decls_p, nm_p, undef_p, out_c, out_s = sys.argv[1:7]
    # names the host already defines (libultraship, src/port, the game library): never given storage here
    host_defined = set(open(sys.argv[7]).read().split()) if len(sys.argv) > 7 else set()
    images = json.load(open(syms_p))
    decls = json.load(open(decls_p))
    undef = set(open(undef_p).read().split())
    host_funcs = set(); host_data = set()
    for line in open(nm_p):
        p = line.split()
        if len(p) == 3 and p[1] in 'T': host_funcs.add(p[2])
        elif len(p) == 3 and p[1] in 'DBRCS': host_data.add(p[2])

    g = Gen(decls['types'])

    image_names = sorted(images, key=lambda n: (n != 'resident', n))
    defined_at = {}
    defined_names_at = collections.defaultdict(list)
    for im in image_names:
        for n, a in images[im]['defined'].items():
            defined_at.setdefault(a, n)
            defined_names_at[a].append(n)

    # all symbol boundaries per image (the size of an asm symbol is the gap to the next one)
    bounds = {}
    for im in image_names:
        v = images[im]
        b = set(v['defined'].values()) | set(v['provided'].values())
        for va, off, size in v['parts']: b.add(va); b.add(va + size)
        for va, size in v['nobits']: b.add(va); b.add(va + size)
        bounds[im] = sorted(b)

    def loaded_part(im, a):
        for va, off, size in images[im]['parts']:
            if va <= a < va + size: return va, off, size
        return None

    def nobits_part(im, a):
        for va, size in images[im]['nobits']:
            if va <= a < va + size: return va, size
        return None

    def score(d):
        t = d['t']
        sc = 0
        base = t['of'] if t['k'] == 'array' else t
        if base['k'] == 'rec': sc += 8
        if g.has_ptr(t): sc += 4
        if t['k'] == 'array' and t['n']: sc += 2
        if t['k'] == 'array': sc += 1
        return (sc, g.n64(t)[0])

    report = collections.Counter()
    syms = []          # dict per lifted symbol
    aliases = []       # (name, target, host offset)
    absyms = []        # (name, value)
    # A link script PROVIDEs every symbol its image references, including ones that live in other images: the owner
    # of a symbol is the image whose loaded or bss ranges contain its address.
    owner_ranges = []
    for im in image_names:
        v = images[im]
        for va, off, size in v['parts']: owner_ranges.append((va, va + size, im))
        for va, size in v['nobits']: owner_ranges.append((va, va + size, im))
    owner_ranges.sort()
    def owner(a):
        for lo, hi, im in owner_ranges:
            if lo <= a < hi: return im
        # bss of the asm files has no section of its own: it lies past an image's loaded parts, in its vram slot
        for im in image_names:
            base = images[im]['vram']
            if im == 'resident' and 0x80000000 <= a < 0x81000000: return im
            if im != 'resident' and (a & 0xFFF00000) == (base & 0xFFF00000): return im
        return None
    provided = {}
    for im in image_names:
        for name, a in images[im]['provided'].items():
            if provided.setdefault(name, a) != a: report['conflicting address ' + name] += 1
    by_image = collections.defaultdict(list)
    for name, a in provided.items():
        by_image[owner(a) or ('-' if a >= 0x10000000 else 'abs')].append((name, a))
    # Data blobs (models, ...) share one vram slot and stay raw big-endian images, as in stadion: the port relocates
    # them to pseudo addresses when the game loads them (Port_RelocateDataBlob). Their symbols stay link addresses.
    slots = collections.Counter(images[im]['vram'] & 0xFFF00000 for im in image_names if im != 'resident')
    blob_images = {im for im in image_names if im != 'resident' and slots[images[im]['vram'] & 0xFFF00000] > 1}
    for im in image_names + ['-', 'abs']:
        v = images.get(im, {'rom': 0, 'parts': [], 'nobits': []})
        for name, a in sorted(by_image[im], key=lambda x: x[1]):
            if name not in undef: continue
            if name in host_defined:
                report['defined by the host'] += 1; continue
            if im in blob_images and not (a in defined_at and defined_at[a] != name):
                absyms.append((name, a)); report['blob symbol (link address)'] += 1; continue
            if a in defined_at and defined_at[a] != name:
                tgt = [n for n in defined_names_at[a] if n in host_defined]
                if tgt:
                    aliases.append((name, tgt[0], 0)); report['alias'] += 1; continue
                if not decls['ext'].get(name):
                    report['alias of code the host lacks: ' + defined_at[a]] += 1; continue
                report['alias of a C static (lifted separately: make it global) ' + defined_at[a]] += 1
            if im == '-':
                report['outside every image: ' + name] += 1; continue
            if a < 0x10000000:
                absyms.append((name, a)); report['abs'] += 1; continue
            ds = decls['ext'].get(name)
            b = bounds[im]
            nxt = b[bisect.bisect_right(b, a)] if bisect.bisect_right(b, a) < len(b) else a + 4
            gap = nxt - a
            if not ds:
                report['no variable declaration (function? left undefined)'] += 1
                continue
            else:
                t = max(ds, key=score)['t']
            if t['k'] == 'function':
                report['function without C (left undefined)'] += 1
                continue
            elem = t['of'] if t['k'] == 'array' else t
            if elem['k'] in ('opaque', 'void'):
                elem = {'k': 's', 'size': 1, 'sg': False}
                report['opaque (bytes)'] += 1
            ti = g.tid(elem)
            en = g.tab[ti][1] or 1
            declared = (t['n'] or 0) if t['k'] == 'array' else 1
            count = max(declared, gap // en, 1)
            lp = loaded_part(im, a)
            nb = nobits_part(im, a) or (lp is None)
            if nb and gap > 0x10000 and count > declared:
                count = max(declared, 0x10000 // en, 1)  # last bss symbol of an image: no next symbol bounds it
            syms.append(dict(name=name, addr=a, image=im, tid=ti, score=score({'t': t}), count=count, n64ext=count * en,
                             hostsize=count * g.tab[ti][2], bss=bool(nb),
                             rom=(lp[1] + (a - lp[0])) if (lp and not nb) else 0))
            report['bss' if nb else ('data' if lp else 'unplaced')] += 1

    # symbols inside the extent of an earlier one become aliases into it
    # several names for one address: the best-typed one owns the storage, the others alias it
    syms.sort(key=lambda x: (x['addr'], tuple(-v for v in x['score'])))
    kept = []
    for sy in syms:
        if kept and sy['addr'] < kept[-1]['addr'] + kept[-1]['n64ext'] and sy['image'] == kept[-1]['image']:
            p = kept[-1]
            ho = g.translate(p['tid'], sy['addr'] - p['addr'])
            if ho is None:
                ho = 0; report['inner alias at padding'] += 1
            aliases.append((sy['name'], p['name'], ho)); report['inner alias'] += 1
            continue
        # clip an extent that runs into the next symbol's start (from a declared bound larger than the gap)
        kept.append(sy)
    syms = kept

    # host functions with an N64 address (function pointers in data)
    funcs = []
    for im in image_names:
        for n, a in images[im]['defined'].items():
            if n in host_funcs: funcs.append((a, n))
    funcs = sorted(set(funcs))

    with open(out_c, 'w') as f:
        w = f.write
        w('// Generated by tools/zero/gen_lift.py from the decomp\'s link maps and C declarations. Do not edit.\n')
        w('// No ROM data here: storage is filled at runtime from the user\'s ROM (src/port/zero_lift.c).\n')
        w('#include <stdint.h>\n#include <stddef.h>\n#include "port/zero_lift.h"\n\n')
        for sy in syms:
            w('ZLIFT_STORAGE(%s, %d);\n' % (sy['name'], max(sy['hostsize'], 1)))
        w('\nconst ZLiftType gZLiftTypes[] = {\n')
        opi = 0; allops = []
        for key, n64s, hs, n64a, ops in g.tab:
            w('    { %d, %d, %d, %d }, // %s\n' % (n64s, hs, opi, len(ops), key[:100]))
            opi += len(ops); allops += ops
        w('};\nconst uint32_t gZLiftTypeCount = %d;\n\n' % len(g.tab))
        w('const ZLiftOp gZLiftOps[] = {\n')
        for o in allops:
            w('    { %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d },\n' % tuple(o))
        if not allops: w('    { 0 },\n')
        w('};\n\n')
        imidx = {n: i for i, n in enumerate(image_names)}
        w('const ZLiftSym gZLiftSyms[] = {\n')
        for sy in syms:
            w('    { 0x%08X, 0x%X, %d, %d, %d, %d, 0x%X, %s }, // %s\n' % (
                sy['addr'], sy['n64ext'], sy['count'], sy['tid'], imidx[sy['image']], 1 if sy['bss'] else 0,
                sy['rom'], sy['name'], sy['name']))
        w('};\nconst uint32_t gZLiftSymCount = %d;\n\n' % len(syms))
        w('const ZLiftImage gZLiftImages[] = {\n')
        for im in image_names:
            v = images[im]
            w('    { "%s", 0x%08X, 0x%X },\n' % (im, v['vram'], v['rom']))
        w('};\nconst uint32_t gZLiftImageCount = %d;\n\n' % len(image_names))
        allb = sorted(set(x for im in image_names for x in bounds[im]))
        w('const uint32_t gZLiftBounds[] = {\n')
        for i in range(0, len(allb), 8):
            w('    ' + ', '.join('0x%08X' % x for x in allb[i:i + 8]) + ',\n')
        w('};\nconst uint32_t gZLiftBoundCount = %d;\n\n' % len(allb))
        for a, n in funcs: w('extern void %s(void);\n' % n)
        w('\nconst ZLiftFunc gZLiftFuncs[] = {\n')
        for a, n in funcs: w('    { 0x%08X, (void*)%s },\n' % (a, n))
        w('};\nconst uint32_t gZLiftFuncCount = %d;\n' % len(funcs))
        # parts: where every image's bytes come from (lazy lifts of unnamed data use it)
        w('\nconst ZLiftPart gZLiftParts[] = {\n')
        nparts = 0
        for im in image_names:
            v = images[im]
            if im in blob_images: continue
            for va, off, size in sorted(v['parts']):
                w('    { 0x%08X, 0x%X, 0x%X, %d },\n' % (va, size, off, imidx[im])); nparts += 1
        w('};\nconst uint32_t gZLiftPartCount = %d;\n' % nparts)

    with open(out_s, 'w') as f:
        w = f.write
        w('// Generated by tools/zero/gen_lift.py. Aliases of the decomp (several names for one address) and\n')
        w('// absolute symbols (ROM offsets, segment addresses) that the game code references.\n')
        w('#if defined(__APPLE__)\n#define SYM(x) _##x\n#else\n#define SYM(x) x\n#endif\n')
        for n, a in absyms:
            w('.globl SYM(%s)\n.set SYM(%s), 0x%X\n' % (n, n, a))
        w('#if defined(__linux__)\n.section .note.GNU-stack,"",@progbits\n#endif\n')

    # Aliases go to the linker (an assembler .set to a symbol of another object would stay undefined)
    with open(out_s.replace('abs_syms.S', 'zero_aliases.ld'), 'w') as f:
        f.write('/* Generated by tools/zero/gen_lift.py: names of the decomp that alias another symbol (same address, or\n')
        f.write('   inside a lifted symbol at the given host offset). Passed to the linker as an implicit script. */\n')
        for n, t, off in aliases:
            f.write('%s = %s + %d;\n' % (n, t, off) if off else '%s = %s;\n' % (n, t))
    for k, n in sorted(report.items()): print('%6d %s' % (n, k))
    print('%6d types, %d ops, %d host functions' % (len(g.tab), sum(len(t[4]) for t in g.tab), len(funcs)))
    for k, n in g.warn.items(): print('warn', n, k)


if __name__ == '__main__':
    main()
