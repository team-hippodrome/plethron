"""Shared helpers for the decomp audit: a comment/string/preprocessor-stripping C tokenizer, top-level definition
extraction (functions and initialised data) and per-function token features. Names are normalised through the
decomp's symbol_addrs (name -> @ADDRESS), so func_XXXXXXXX in one tree matches its real name in the other."""
import re,sys,os,collections,json
KW={'if','else','for','while','do','switch','case','default','return','break','continue','goto'}
TYPES=set('''void char short int long float double signed unsigned const volatile static extern struct union enum
typedef sizeof u8 s8 u16 s16 u32 s32 u64 s64 f32 f64 uintptr_t intptr_t size_t ptrdiff_t uint8_t uint16_t uint32_t
int8_t int16_t int32_t int64_t uint64_t UNUSED M2C_UNK M2C_UNK8 M2C_UNK16 M2C_UNK32 M2C_UNK64 bool inline register
uint ulong ushort uchar'''.split())
def strip(s):
    s=re.sub(r'/\*.*?\*/',' ',s,flags=re.S); s=re.sub(r'//[^\n]*','',s)
    s=re.sub(r'"(\\.|[^"\\\n])*"','""',s); s=re.sub(r"'(\\.|[^'\\\n])*'","'c'",s)
    # drop preprocessor lines (incl. continuations)
    s=re.sub(r'^[ \t]*#(?:[^\n]*\\\n)*[^\n]*','',s,flags=re.M)
    return s
TOK=re.compile(r'0[xX][0-9a-fA-F]+[uUlL]*|\d+\.\d*(?:[eE][-+]?\d+)?[fF]?|\.\d+(?:[eE][-+]?\d+)?[fF]?|\d+(?:[eE][-+]?\d+)?[fFuUlL]*|[A-Za-z_]\w*|->|\+\+|--|<<=|>>=|[-+*/%&|^]=|==|!=|<=|>=|&&|\|\||<<|>>|.',re.S)
def num(t):
    t2=t.rstrip('uUlL')
    try:
        if t2.lower().startswith('0x'): return 'N%d'%int(t2,16)
        if re.fullmatch(r'\d+',t2): return 'N%d'%int(t2,10)
        return 'F%g'%float(t2.rstrip('fF'))
    except Exception: return 'N?'+t
def defs(path):
    s=strip(open(path,errors='replace').read()); out={}
    # functions and initialised data at top level
    depth=0;i=0;n=len(s);start=0
    toks=[(m.group(0),m.start()) for m in TOK.finditer(s) if not m.group(0).isspace()]
    # walk top-level: collect chunks ending at ';' or matching '}' at depth 0
    chunk=[];j=0
    while j<len(toks):
        t,_=toks[j]
        if t=='{': depth+=1
        elif t=='}':
            depth-=1
            if depth==0:
                chunk.append(t); j+=1
                if j<len(toks) and toks[j][0]==';': chunk.append(';'); j+=1
                add(out,chunk); chunk=[]; continue
        elif t==';' and depth==0:
            chunk.append(t); add(out,chunk); chunk=[]; j+=1; continue
        chunk.append(t); j+=1
    return out
def add(out,chunk):
    if '{' not in chunk: return
    k=chunk.index('{')
    head=chunk[:k]
    if '=' in head:  # data initialiser
        e=head.index('='); h=head[:e]
        names=[t for t in h if re.fullmatch(r'[A-Za-z_]\w*',t) and t not in TYPES]
        if not names: return
        # skip [..] dims: name is the first ident before '['
        name=names[-1]
        for idx,t in enumerate(h):
            if t=='[' : name=[x for x in h[:idx] if re.fullmatch(r'[A-Za-z_]\w*',x)][-1]; break
        out['data:'+name]=('data',chunk[k:])
    elif '(' in head and head[-1]==')' or (')' in head):
        p=head.index('(')
        # function pointer returning functions are rare; take ident before first '('
        names=[t for t in head[:p] if re.fullmatch(r'[A-Za-z_]\w*',t)]
        if not names: return
        if names[0] in ('struct','union','enum','typedef'): return
        out[names[-1]]=('func',chunk[k:])
def feats(body,ren):
    c=collections.Counter()
    b=body
    for i,t in enumerate(b):
        nxt=b[i+1] if i+1<len(b) else ''
        if re.fullmatch(r'[A-Za-z_]\w*',t):
            orig=t; t=ren.get(t,t)
            mm=re.fullmatch(r'(?:func|D|B|jtbl)_([0-9A-Fa-f]{8})',t)
            if mm: t='@'+mm.group(1).upper()
            if t in KW: c['kw:'+t]+=1
            elif t in TYPES: pass
            elif nxt=='(' and not orig.isupper(): c['call:'+t]+=1
            else: c['id:'+t]+=1
        elif re.match(r'[\d.]',t) and t!='.': c['num:'+num(t)]+=1
        elif t==';': c['stmt']+=1
        elif t in ('=','+=','-=','*=','/=','|=','&=','^=','<<=','>>=','%=','++','--'): c['asg']+=1
    return c
def load_renames(symbol_addrs_paths):
    """{name: '@ADDRESS'} from pokestadium0's linker_scripts/jp/symbol_addrs.txt (one or more versions)."""
    ren = {}
    for p in symbol_addrs_paths:
        for l in open(p, errors='replace'):
            m = re.match(r'\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)', l)
            if m:
                ren[m.group(1)] = '@' + m.group(2).upper()
    return ren


def walk_pairs(base, port):
    """(relative path, base defs, port defs) for every .c file of base that the port also has."""
    for root, _, fs in os.walk(base):
        for f in sorted(fs):
            if not f.endswith('.c'):
                continue
            bp = os.path.join(root, f)
            rel = os.path.relpath(bp, base)
            pp = os.path.join(port, rel)
            if not os.path.exists(pp):
                yield rel, defs(bp), None
            else:
                yield rel, defs(bp), defs(pp)
