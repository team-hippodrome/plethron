#!/usr/bin/env python3
# Re-wraps translated move descriptions (*_MOVE_DESC) whose lines overflow the Party summary's description box
# (MonSummary_DrawMoveInfo: 16 px glyphs from x+9 to x+282, 3 lines), keeping every word: lines are only refilled.
# Lines that still overflow after refilling are listed for the human localizers (AI text is never shortened).
# usage: reflow_move_desc.py <unifont_subset.hex> translations/ai.csv [--write]
import csv,io,sys
hexf,csvf=sys.argv[1],sys.argv[2]
W={}
for l in open(hexf):
    if ':' in l and not l.startswith('#'):
        a,b=l.strip().split(':'); W[int(a,16)]=8 if len(b)==32 else 16
def adv(c,h=16):
    w=W.get(ord(c),W.get(ord('?'),8)); sw=w+2; s=h/18
    return max(1,round(sw*s+1e-9) if (sw*s)%1!=0.5 else int(sw*s)+1) - (int(s)+1 if s%1>=0.5 else int(s))
AVAIL,LINES=273,3
def width(t): return sum(adv(c) for c in t)
def fits(t): return width(t)<=AVAIL
def squish(t): return (width(t)-AVAIL)*len(t)<=2*width(t)
def wrap(text,ok):
    lines=[]
    for w in text.split():
        if lines and ok(lines[-1]+' '+w): lines[-1]+=' '+w
        else: lines.append(w)
    return lines
src=open(csvf,encoding='utf-8',newline='').read()
out=src
for row in csv.DictReader(io.StringIO(src)):
    if not row['id'].endswith('_MOVE_DESC'): continue
    for L in ['en','es','de','pt','ko']:
        old=row[L]
        if all(squish(x) for x in old.split('\n')): continue  # over by 2 characters or less: Port_Font_FitNext squishes it
        new=wrap(old,fits)
        if len(new)>LINES: new=wrap(old,squish)
        bad=[x for x in new if not squish(x)]
        if len(new)>LINES or bad:
            print('STILL OVER',row['id'],L,repr(old)); continue
        new='\n'.join(new)
        print(row['id'],L,repr(old),'->',repr(new))
        q=lambda s:'"'+s.replace('"','""')+'"'
        assert out.count(q(old))>=1
        out=out.replace(','+q(old)+',',','+(q(new) if ('\n' in new or ',' in new or '"' in new) else new)+',',1)
if '--write' in sys.argv: open(csvf,'w',encoding='utf-8',newline='').write(out)
