#!/usr/bin/env python3
"""Check translations/ai.csv and translations/human.csv.

  python3 tools/text/check_tables.py              format checks only (CI-safe, no ROM)
  python3 tools/text/check_tables.py --rom X.z64  also check keys and printf
                                                  specifiers against your ROM

Format checks: exact header, one row per id, ids are [A-Z][A-Z0-9_]*, md5s
are lowercase 32-hex, addr is space-separated 8-hex-digit addresses, every
language cell in a row uses the same printf conversions in the same order
(positional %1$s forms excepted, as long as the same set is used).
ROM checks: every md5 exists in the ROM, every human.csv id is in ai.csv with
the same md5, and each translation's conversions match the original string's.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import tables  # noqa: E402


def same_specs(a, b):
    sa, sb = tables.specs(a), tables.specs(b)
    if any("$" in s for s in sa + sb):
        strip = lambda xs: sorted(s.split("$", 1)[-1] if "$" in s else s for s in xs)
        return strip(sa) == strip(sb)
    return sa == sb


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rom")
    a = ap.parse_args()
    problems = []
    data = {}
    for name in ("ai", "human"):
        rows, p = tables.read(name)
        problems += p
        data[name] = rows
        for md5, row in rows.items():
            filled = [(l, row[l]) for l in tables.LANGS if row[l]]
            for l, text in filled[1:]:
                if not same_specs(filled[0][1], text):
                    problems.append("%s.csv %s: %s and %s use different printf conversions" % (name, md5, filled[0][0], l))
            for l, text in filled:
                if "\r" in text:
                    problems.append("%s.csv %s %s: carriage return in cell" % (name, md5, l))
    if a.rom:
        import romtext
        rom, sha1, ok = romtext.load_rom(a.rom)
        if not ok:
            problems.append("ROM sha1 %s is not the supported dump %s" % (sha1, romtext.SUPPORTED_SHA1))
        found = romtext.extract(rom)
        for name, rows in data.items():
            for key, row in rows.items():
                md5 = row["md5"]
                if md5 not in found:
                    problems.append("%s.csv %s: md5 not found in the ROM" % (name, key))
                    continue
                orig = found[md5][2].decode("euc-jp")
                for l in tables.LANGS:
                    if row[l] and not same_specs(orig, row[l]):
                        problems.append("%s.csv %s %s: printf conversions differ from the original" % (name, key, l))
        ai = data["ai"]
        for key, row in data["human"].items():
            if key not in ai:
                problems.append("human.csv %s: id not in ai.csv" % key)
            elif ai[key]["md5"] != row["md5"]:
                problems.append("human.csv %s: md5 differs from ai.csv" % key)
        ids = tables.by_md5(ai)
        print("coverage vs ROM (%d strings):" % len(found))
        for l in tables.LANGS:
            n = sum(1 for m in found
                    if m in ids and all(tables.effective(k, l, data["human"], ai)[0] for k in ids[m]))
            print("  %s %4d / %d" % (l, n, len(found)))
    for p in problems:
        print("error:", p)
    print("%d problem(s)" % len(problems))
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
