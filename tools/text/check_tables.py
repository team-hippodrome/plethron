#!/usr/bin/env python3
"""Check translations/ai.csv and translations/human.csv.

  python3 tools/text/check_tables.py              format checks only (CI-safe, no ROM)
  python3 tools/text/check_tables.py --rom X.z64  also check keys and printf
                                                  specifiers against your ROM

Format checks: exact header, one row per md5, lowercase 32-hex keys, every
language cell in a row uses the same printf conversions in the same order
(positional %1$s forms excepted, as long as the same set is used).
ROM checks: every key exists in the ROM, and each translation's conversions
match the original string's.
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
            for md5, row in rows.items():
                if md5 not in found:
                    problems.append("%s.csv %s: key not found in the ROM" % (name, md5))
                    continue
                orig = found[md5][2].decode("euc-jp")
                for l in tables.LANGS:
                    if row[l] and not same_specs(orig, row[l]):
                        problems.append("%s.csv %s %s: printf conversions differ from the original" % (name, md5, l))
        ai = data["ai"]
        print("coverage vs ROM (%d strings):" % len(found))
        for l in tables.LANGS:
            n = sum(1 for m in found if tables.effective(m, l, data["human"], ai)[0])
            print("  %s %4d / %d" % (l, n, len(found)))
    for p in problems:
        print("error:", p)
    print("%d problem(s)" % len(problems))
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
