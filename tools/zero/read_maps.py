#!/usr/bin/env python3 -I
"""Reads the per-image link maps of a Stadium Zero decomp build (pokestadium0: `make rom`, which writes
build/production/<image>/link.map) into one JSON table: for every image (resident, fragment000..258) its ROM range,
link address and every symbol the image defines or PROVIDEs. The port's generators read this table instead of the
maps, so they run without a decomp checkout once it exists.

Usage: tools/zero/read_maps.py <pokestadium0 checkout> <out.json>"""
import json, re, sys, pathlib

dec = pathlib.Path(sys.argv[1])
out = {}
sym = re.compile(r"^\s+0x([0-9a-f]{8,16})\s+([A-Za-z_.$][\w.$]*)\s*$")
prov = re.compile(r"^\s+0x([0-9a-f]{8,16})\s+PROVIDE \(([A-Za-z_]\w*) = 0x[0-9a-f]+\)")
part = re.compile(r"^\.part_\d+\s+0x([0-9A-Fa-f]+)\s*:\s*AT\((\d+)\)")
for img in sorted((dec / "build/production").iterdir()):
    mp = img / "link.map"
    if not mp.exists():
        continue
    defined, provided = {}, {}
    cur_obj = None
    for line in mp.read_text(errors="replace").splitlines():
        m = prov.match(line)
        if m:
            provided[m.group(2)] = int(m.group(1), 16)
            continue
        m = sym.match(line)
        if m:
            defined.setdefault(m.group(2), int(m.group(1), 16))
    ld = (img / "link.ld").read_text(errors="replace")
    starts = [int(m.group(1), 16) for m in re.finditer(r"^\.part_\d+\s+0x([0-9A-Fa-f]+)", ld, re.M)]
    out[img.name] = {"vram": min(starts) if starts else None, "defined": defined, "provided": provided}
json.dump(out, open(sys.argv[2], "w"), indent=1, sort_keys=True)
print(len(out), "images,", sum(len(v["defined"]) for v in out.values()), "defined symbols")
