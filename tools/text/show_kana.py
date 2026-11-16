#!/usr/bin/env python3
"""Show the game's original Japanese next to the translation tables.

Reads YOUR ROM and writes into translations/rom-text/ (gitignored, never commit it):

  kana.csv     md5, where, kana              every string the game prints
  review.csv   id, md5, addr, where, kana, ja..ko, from, note
               the translation the port will show for each language
               (human.csv wins over ai.csv), and which table it came from
  index.html   the same table in a browser, with a search box

Usage:  python3 tools/text/show_kana.py path/to/rom.z64 [--all-blobs]
"""
import argparse
import csv
import html
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import romtext  # noqa: E402
import tables  # noqa: E402

OUT = os.path.join(tables.TDIR, "rom-text")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("rom")
    ap.add_argument("--all-blobs", action="store_true", help="decompress every Yay0 archive, not just the known text ones (slow)")
    a = ap.parse_args()

    rom, sha1, ok = romtext.load_rom(a.rom)
    if not ok:
        print("warning: ROM sha1 %s is not the supported Pokémon Stadium (Japan) dump %s; keys may not match"
              % (sha1, romtext.SUPPORTED_SHA1), file=sys.stderr)
    found = romtext.extract(rom, all_blobs=a.all_blobs)
    ai, p1 = tables.read("ai")
    human, p2 = tables.read("human")
    for p in p1 + p2:
        print("warning:", p, file=sys.stderr)

    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, ".gitignore"), "w") as f:
        f.write("# Text dumped from your own ROM. Never commit it.\n*\n")

    records = []
    ids = tables.by_md5(ai)
    for md5, (src, off, raw) in found.items():
        kana = romtext.to_cell(raw.decode("euc-jp"))
        where = "%s+%06X" % (src, off) if src != "rom" else "%06X" % off
        for key in ids.get(md5, [""]):
            addr = (ai.get(key) or human.get(key) or {}).get("addr", "")
            rec = {"id": key, "md5": md5, "addr": addr, "where": where, "kana": kana}
            sources = []
            for lang in tables.LANGS:
                text, is_human = tables.effective(key, lang, human, ai)
                rec[lang] = text
                if text:
                    sources.append("%s:%s" % (lang, "human" if is_human else "ai"))
            rec["from"] = " ".join(sources)
            rec["note"] = (human.get(key) or {}).get("note") or (ai.get(key) or {}).get("note") or ""
            records.append(rec)

    with open(os.path.join(OUT, "kana.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["md5", "where", "kana"])
        for r in records:
            if r["id"] == (ids.get(r["md5"]) or [""])[0]:
                w.writerow([r["md5"], r["where"], r["kana"]])
    cols = ["id", "md5", "addr", "where", "kana"] + tables.LANGS + ["from", "note"]
    with open(os.path.join(OUT, "review.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(cols)
        for r in records:
            w.writerow([r[c] for c in cols])
    write_html(os.path.join(OUT, "index.html"), cols, records)

    hmd5 = tables.by_md5({k: r for k, r in human.items() if any(r[l] for l in tables.LANGS)})
    print("%d strings from the ROM -> %s" % (len(found), OUT))
    print("  in ai.csv: %d   translated in human.csv: %d   in neither: %d" % (
        sum(1 for m in found if m in ids), sum(1 for m in found if m in hmd5),
        sum(1 for m in found if m not in ids and m not in hmd5)))


def write_html(p, cols, records):
    rows = json.dumps([[r[c] for c in cols] for r in records], ensure_ascii=False)
    head = "".join("<th>%s</th>" % html.escape(c) for c in cols)
    page = """<!doctype html><meta charset="utf-8"><title>Stadium Zero text</title>
<style>
body{font:14px system-ui,sans-serif;margin:16px;background:#fff;color:#111}
@media (prefers-color-scheme:dark){body{background:#151515;color:#eee}td,th{border-color:#444!important}}
input{width:100%%;padding:6px;font-size:15px;margin-bottom:8px;box-sizing:border-box}
table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:3px 6px;vertical-align:top;white-space:pre-wrap}
th{position:sticky;top:0;background:inherit}td:nth-child(-n+2){font:11px monospace}
td[dir=rtl]{text-align:right}
</style>
<p>Text from your own ROM. Translations shown are what the port uses (human.csv before ai.csv). Do not share or commit this file.</p>
<input id=q placeholder="Search any column (kana, English, id…)" autofocus>
<table><thead><tr>%s</tr></thead><tbody id=b></tbody></table>
<script>
const C=%s,R=%s,b=document.getElementById('b');
function draw(f){f=f.toLowerCase();let h='';for(const r of R){if(f&&!r.join('\\u0001').toLowerCase().includes(f))continue;
h+='<tr>'+r.map((v,i)=>'<td'+(C[i]=='ar'?' dir=rtl':'')+'>'+v.replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]))+'</td>').join('')+'</tr>'}b.innerHTML=h}
document.getElementById('q').oninput=e=>draw(e.target.value);draw('');
</script>""" % (head, json.dumps(cols), rows)
    with open(p, "w", encoding="utf-8") as f:
        f.write(page)


if __name__ == "__main__":
    main()
