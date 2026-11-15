# Collect variable types (extern declarations of wanted names + file-scope definitions)
# from every game TU with libclang. Records go into a shared type table keyed by name+layout hash.
import clang.cindex as ci, json, sys, pathlib, hashlib
from concurrent.futures import ProcessPoolExecutor
root = pathlib.Path('/home/user/plethron')
flags = [l.strip() for l in open(root/'tools/compile_flags.txt') if l.strip()]
args = []
for f in flags:
    if f.startswith('-I'): args.append('-I' + str(root / f[2:]))
    elif f.endswith('port_compat.h'): args.append(str(root / f))
    else: args.append(f)
args = [a for a in args if not (a.startswith('-W') and not a.startswith('-Wno-')) and a not in ('-ferror-limit=0',)] + ['-w', '-isystem', '/usr/lib/llvm-18/lib/clang/18/include']
want = set(open(sys.argv[1]).read().split())
K = ci.TypeKind

class Ctx:
    def __init__(s): s.types = {}; s.busy = {}

def tref(t, cx):
    t = t.get_canonical()
    k = t.kind
    if k == K.CONSTANTARRAY:
        return {'k': 'array', 'n': t.element_count, 'of': tref(t.element_type, cx)}
    if k in (K.INCOMPLETEARRAY, K.VARIABLEARRAY, K.DEPENDENTSIZEDARRAY):
        return {'k': 'array', 'n': None, 'of': tref(t.element_type, cx)}
    if k == K.POINTER:
        pt = t.get_pointee().get_canonical()
        if pt.kind in (K.FUNCTIONPROTO, K.FUNCTIONNOPROTO): return {'k': 'fnptr'}
        if pt.kind == K.VOID: return {'k': 'ptr', 'to': None}
        return {'k': 'ptr', 'to': tref(pt, cx)}
    if k == K.RECORD:
        if t.get_size() < 0: return {'k': 'opaque', 'name': t.spelling}
        return {'k': 'rec', 'id': record(t, cx)}
    if k == K.ENUM:
        return {'k': 's', 'size': t.get_size(), 'sg': True}
    if k in (K.LONG, K.ULONG):  # uintptr_t, intptr_t, size_t: 32 bits on the N64, often an address
        return {'k': 'iptr'}
    if k in (K.FLOAT, K.DOUBLE, K.LONGDOUBLE):
        return {'k': 'f', 'size': t.get_size()}
    if k in (K.FUNCTIONPROTO, K.FUNCTIONNOPROTO): return {'k': 'function'}
    if k == K.VOID: return {'k': 'void'}
    return {'k': 's', 'size': t.get_size(), 'sg': k in (K.CHAR_S, K.SCHAR, K.SHORT, K.INT, K.LONG, K.LONGLONG)}

def record(t, cx):
    decl = t.get_declaration()
    key0 = (t.spelling, decl.location.file.name if decl.location.file else '', decl.location.line)
    if key0 in cx.busy: return cx.busy[key0]
    # provisional id from name+location, replaced by layout-hash id below
    name = t.spelling
    loc = '%s:%d' % (pathlib.Path(key0[1]).name, key0[2])
    rid = name + '@' + loc
    cx.busy[key0] = rid
    fields = []
    for c in t.get_fields():  # includes the unnamed fields of anonymous struct/union members
        if True:
            f = {'name': c.spelling, 'bit': c.get_field_offsetof(), 't': tref(c.type, cx)}
            if c.is_bitfield(): f['w'] = c.get_bitfield_width()
            fields.append(f)
    cx.types[rid] = {'k': 'union' if decl.kind == ci.CursorKind.UNION_DECL else 'struct', 'name': name,
                     'size': t.get_size(), 'align': t.get_align(), 'fields': fields}
    return rid

def one(path):
    idx = ci.Index.create()
    try: tu = idx.parse(str(path), args=args)
    except Exception as e: return str(path), {}, {}, {}, [str(e)]
    cx = Ctx(); ext = {}; defs = {}
    def walk(cur, top):
        for c in cur.get_children():
            if c.location.file and not str(c.location.file).startswith(str(root)): continue
            if c.kind == ci.CursorKind.VAR_DECL:
                yield c, top
            elif c.kind in (ci.CursorKind.FUNCTION_DECL, ci.CursorKind.COMPOUND_STMT, ci.CursorKind.DECL_STMT) or \
                    c.kind.is_statement():
                yield from walk(c, False)
    for c, top in walk(tu.cursor, True):
        if not top and c.storage_class != ci.StorageClass.EXTERN: continue
        if c.storage_class == ci.StorageClass.EXTERN and not c.is_definition():
            if c.spelling in want:
                ext.setdefault(c.spelling, []).append({'sp': c.type.spelling, 't': tref(c.type, cx)})
        elif c.is_definition() and c.storage_class != ci.StorageClass.STATIC:
            defs[c.spelling] = {'sp': c.type.spelling, 't': tref(c.type, cx)}
    errs = [str(d) for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error][:3]
    return str(path), ext, defs, cx.types, errs

if __name__ == '__main__':
    files = [p for p in sorted((root/'game/src').rglob('*.c')) if '/libultra/' not in str(p)]
    ext = {}; defs = {}; types = {}
    with ProcessPoolExecutor(4) as ex:
        for path, e, d, ty, errs in ex.map(one, files):
            rel = path.replace(str(root)+'/', '')
            if errs: print('ERR', rel, errs[:1], file=sys.stderr)
            types.update(ty)
            for n, v in e.items():
                for x in v: ext.setdefault(n, []).append(dict(x, file=rel))
            for n, x in d.items(): defs[n] = dict(x, file=rel)
    json.dump({'ext': ext, 'defs': defs, 'types': types}, open(sys.argv[2], 'w'))
    print(len(ext), 'extern', len(defs), 'defs', len(types), 'record types')
