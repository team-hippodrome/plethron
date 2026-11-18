#!/usr/bin/env python3
"""Writes assets/font/text3d_subset.otf: the Noto Sans CJK JP Bold outlines of every character the MODEL_TEXT_* rows of
the translation tables use (the words the game draws as 3D models: round titles, かち / まけ).

The port extrudes these outlines into bevelled meshes like the game's own letters (src/port/text3d.cpp). Run it again
after a MODEL_TEXT_* cell gains characters:
    python3 tools/font/make_text3d_subset.py [--font /usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc]
--check only reports characters missing from the committed subset (exit 1 when any are).

Needs fontTools (pip install fonttools) and the Noto CJK fonts (Ubuntu: apt install fonts-noto-cjk). Noto Sans CJK is
licensed under the SIL Open Font License 1.1 (assets/font/LICENSE-noto-cjk.txt). No ROM data goes into this file.
"""
import argparse
import csv
import os
import sys

from fontTools import subset
from fontTools.ttLib import TTCollection, TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "assets", "font", "text3d_subset.otf")
TABLES = [os.path.join(ROOT, "translations", n) for n in ("ai.csv", "human.csv", "localized.csv")]
TEXT_COLUMNS = ("ja", "en", "es", "de", "pt", "ko", "fr", "it")
PREFIX = "MODEL_TEXT_"
FAMILY = "Noto Sans CJK JP"  # the Japanese glyph shapes for kanji; Latin and hangul are the same in every member


def wanted_chars():
    chars = set()
    for path in TABLES:
        with open(path, newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                if not row["id"].startswith(PREFIX):
                    continue
                for col in TEXT_COLUMNS:
                    chars.update(row.get(col) or "")
    chars.add(" ")  # word spaces (the mesh builder only needs its advance)
    return sorted(c for c in chars if c == " " or not c.isspace())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--font", default="/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    chars = wanted_chars()
    if args.check:
        have = set(TTFont(OUT).getBestCmap())
        missing = [c for c in chars if ord(c) not in have]
        if missing:
            print("missing from text3d_subset.otf:", "".join(missing))
            return 1
        print(f"text3d_subset.otf has all {len(chars)} characters")
        return 0
    font = None
    for f in TTCollection(args.font).fonts:
        if f["name"].getDebugName(1) == FAMILY:
            font = f
            break
    if font is None:
        sys.exit(f"{FAMILY} not found in {args.font}")
    opts = subset.Options()
    opts.layout_features = []  # no shaping in the mesh builder: plain cmap lookups and advance widths
    opts.name_IDs = ["*"]
    opts.notdef_outline = True
    opts.desubroutinize = True
    opts.hinting = False
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=[ord(c) for c in chars])
    sub.subset(font)
    font.save(OUT)
    print(f"{OUT}: {len(chars)} characters, {os.path.getsize(OUT)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
