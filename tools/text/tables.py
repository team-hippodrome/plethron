"""Read and write translations/ai.csv, translations/human.csv and translations/localized.csv.

Each row is one string the game prints, keyed by a unique `id`. `md5` is the
md5 of the original bytes and is the fallback key: when the port can't tell
which id a string is, it looks it up by md5, and if several rows share an md5
(identical bytes with different meanings, e.g. HAUNTER_NAME and
GHOST_TYPE_NAME) the first one in the file wins. `addr` lists the N64 link
addresses the string sits at, so the port can bind ids without a hand-written
table; it is empty for text that only exists inside compressed archives.
"""
import csv
import os
import re

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
TDIR = os.path.join(ROOT, "translations")
LANGS = ["ja", "en", "es", "de", "pt", "ko"]
HEADER = ["id", "md5", "addr"] + LANGS + ["note"]
# localized.csv (the localized form of the name rows, Options > Names) also shows each name's kana
LOCALIZED_HEADER = ["id", "md5", "addr", "kana"] + LANGS + ["note"]


def header(name):
    return LOCALIZED_HEADER if name == "localized" else HEADER
ID_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")
MD5_RE = re.compile(r"^[0-9a-f]{32}$")
ADDR_RE = re.compile(r"^(?:[0-9A-F]{8}(?: [0-9A-F]{8})*)?$")
# printf conversions the game uses (%s, %d, %3d, %02d, %c, %%, %1$s ...)
SPEC_RE = re.compile(r"%(?:\d+\$)?[-+ 0#]*\d*(?:\.\d+)?[hl]*[diouxXcsf%]")


def path(name):
    return os.path.join(TDIR, name + ".csv")


def read(name):
    """Return (ordered dict id -> row dict, list of problems)."""
    rows, problems = {}, []
    p = path(name)
    if not os.path.exists(p):
        return rows, problems
    with open(p, newline="", encoding="utf-8") as f:
        r = csv.reader(f)
        first = next(r, None)
        want = header(name)
        if first != want:
            problems.append("%s.csv: header must be %s" % (name, ",".join(want)))
            return rows, problems
        for n, rec in enumerate(r, start=2):
            if len(rec) != len(want):
                problems.append("%s.csv row %d: %d columns, want %d" % (name, n, len(rec), len(want)))
                continue
            row = dict(zip(want, rec))
            if not ID_RE.match(row["id"]):
                problems.append("%s.csv row %d: bad id %r" % (name, n, row["id"]))
                continue
            if not MD5_RE.match(row["md5"]):
                problems.append("%s.csv row %d: bad md5 %r" % (name, n, row["md5"]))
                continue
            if not ADDR_RE.match(row["addr"]):
                problems.append("%s.csv row %d: bad addr %r" % (name, n, row["addr"]))
                continue
            if row["id"] in rows:
                problems.append("%s.csv row %d: duplicate id %s" % (name, n, row["id"]))
                continue
            rows[row["id"]] = row
    return rows, problems


def by_md5(rows):
    """md5 -> list of ids in file order (the first one is the md5 fallback)."""
    out = {}
    for i, row in rows.items():
        out.setdefault(row["md5"], []).append(i)
    return out


def write(name, rows):
    with open(path(name), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(header(name))
        for row in rows:
            w.writerow([row.get(k, "") for k in header(name)])


def specs(s):
    return [m for m in SPEC_RE.findall(s) if m != "%%"]


def effective(key, lang, human, ai):
    """The cell the runtime will use for an id: human first, then AI, else '' (original)."""
    for table in (human, ai):
        row = table.get(key)
        if row and row[lang]:
            return row[lang], table is human
    return "", False
