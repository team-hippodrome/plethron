#!/usr/bin/env python3
# Estimates whether translated battle HUD labels fit their boxes, with the same glyph sizing as
# src/port/unicode_font.cpp and the squish rule of Port_Font_FitNext (game/src/font.c).
# usage: check_label_fit.py <unifont_subset.hex> translations/ai.csv   prints labels that squish or overflow (OVER)
import csv,sys
hexf,csvf=sys.argv[1],sys.argv[2]
W={}
for l in open(hexf):
    if ':' in l and not l.startswith('#'):
        a,b=l.strip().split(':'); W[int(a,16)]=8 if len(b)==32 else 16
def adv(c,h):
    w=W.get(ord(c),W.get(ord('?'),8)); sw=w+2; s=h/18
    return max(1,round(sw*s+1e-9) if (sw*s)%1!=0.5 else int(sw*s)+1) - (int(s)+1 if s%1>=0.5 else int(s))
def fit(t,h,avail):
    width=sum(adv(c,h) for c in t); n=len(t)
    return width, 'fits' if width<=avail else 'squish' if (width-avail)*n<=2*width else 'OVER'
for row in csv.DictReader(open(csvf,encoding='utf-8')):
    i=row['id']
    if i.endswith('_STATUS_NAME'): h,a=10,37
    elif i.startswith('COM_'): h,a=12,66
    else: continue
    for L in ['en','es','de','pt','ko','fr','it']:
        wd,r=fit(row[L],h,a)
        if r!='fits': print(r,i,L,row[L],wd,a)
