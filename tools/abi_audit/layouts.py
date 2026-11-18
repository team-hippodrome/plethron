"""Compares the record layouts of every game and port C file between x86_64 Linux and x86_64 MinGW (Windows).

The data lifter's tables (src/port/generated/zero_lift_data.c) hold host layouts computed on Linux, and the game
code was tested on Linux, so any record laid out differently on Windows is a Windows-only bug. The game is built with
-mno-ms-bitfields on Windows (CMakeLists.txt), and this check applies the same flag to game files.

usage: tools/abi_audit/layouts.py <compile_commands.json of a Linux build>   (ninja -C build -t compdb > cdb.json)
env:   MINGW_CC (default x86_64-w64-mingw32-clang, from llvm-mingw), HOST_CC (default clang)
Prints the records whose layout differs and the files that use them; no output after "errors: 0" means none.
"""
import os, json, re, subprocess, sys, shlex, collections, concurrent.futures as cf
MINGW = os.environ.get('MINGW_CC', 'x86_64-w64-mingw32-clang')
CLANG = os.environ.get('HOST_CC', 'clang')
db = json.load(open(sys.argv[1]))
tus = [e for e in db if e['file'].endswith('.c') and ('/game/' in e['file'] or '/src/port/' in e['file'])]
def args_for(e, cc):
    a = shlex.split(e['command'])[1:]
    out = []; skip = False
    for i, x in enumerate(a):
        if skip: skip = False; continue
        if x in ('-o', '-MF', '-MT'): skip = True; continue
        if x in ('-c', '-MD', e['file']) or x.startswith('-fPI') or x.startswith('-g'): continue
        out.append(x)
    extra = ['-mno-ms-bitfields'] if (cc != CLANG and '/game/' in e['file']) else []
    return [cc] + out + extra + ['-fsyntax-only', '-w', '-Xclang', '-fdump-record-layouts-simple', e['file']]
def parse(txt):
    recs = {}
    for blk in txt.split('*** Dumping AST Record Layout')[1:]:
        m = re.search(r'Type: (.+)', blk)
        if not m: continue
        name = m.group(1).strip()
        if 'anonymous' in name or 'unnamed' in name: continue
        body = blk.split('Layout: <ASTRecordLayout', 1)
        lay = re.sub(r'\s+', ' ', body[1] if len(body) > 1 else '')
        recs[name] = lay
    return recs
def run(e):
    r = {}
    for tgt, cc in (('linux', CLANG), ('mingw', MINGW)):
        p = subprocess.run(args_for(e, cc), capture_output=True, text=True, cwd=e['directory'])
        r[tgt] = parse(p.stdout)
        if p.returncode: r[tgt + '_err'] = p.stderr[-400:]
    return e['file'], r
diffs = collections.defaultdict(set)
errs = []
with cf.ThreadPoolExecutor(16) as ex:
    for f, r in ex.map(run, tus):
        if 'mingw_err' in r or 'linux_err' in r: errs.append((f, r.get('mingw_err', '') or r.get('linux_err')))
        for k, v in r['linux'].items():
            w = r['mingw'].get(k)
            if w is not None and w != v: diffs[k].add(f.split('plethron/')[-1])
print('errors:', len(errs))
for f, e in errs[:5]: print(' ', f, e[-300:])
for k in sorted(diffs): print(k, '|', len(diffs[k]), sorted(diffs[k])[:3])
