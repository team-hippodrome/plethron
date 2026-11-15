"""Read and write translations/ai.csv and translations/human.csv."""
import csv
import os
import re

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
TDIR = os.path.join(ROOT, "translations")
LANGS = ["ja", "en", "es", "de", "pt", "ko", "ru", "ar", "hi"]
HEADER = ["md5"] + LANGS + ["note"]
MD5_RE = re.compile(r"^[0-9a-f]{32}$")
# printf conversions the game uses (%s, %d, %3d, %02d, %c, %%, %1$s ...)
SPEC_RE = re.compile(r"%(?:\d+\$)?[-+ 0#]*\d*(?:\.\d+)?[hl]*[diouxXcsf%]")


def path(name):
    return os.path.join(TDIR, name + ".csv")


def read(name):
    """Return (ordered dict md5 -> row dict, list of problems)."""
    rows, problems = {}, []
    p = path(name)
    if not os.path.exists(p):
        return rows, problems
    with open(p, newline="", encoding="utf-8") as f:
        r = csv.reader(f)
        header = next(r, None)
        if header != HEADER:
            problems.append("%s.csv: header must be %s" % (name, ",".join(HEADER)))
            return rows, problems
        for n, rec in enumerate(r, start=2):
            if len(rec) != len(HEADER):
                problems.append("%s.csv row %d: %d columns, want %d" % (name, n, len(rec), len(HEADER)))
                continue
            row = dict(zip(HEADER, rec))
            if not MD5_RE.match(row["md5"]):
                problems.append("%s.csv row %d: bad md5 %r" % (name, n, row["md5"]))
                continue
            if row["md5"] in rows:
                problems.append("%s.csv row %d: duplicate md5 %s" % (name, n, row["md5"]))
                continue
            rows[row["md5"]] = row
    return rows, problems


def write(name, rows):
    with open(path(name), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(HEADER)
        for row in rows:
            w.writerow([row.get(k, "") for k in HEADER])


def specs(s):
    return [m for m in SPEC_RE.findall(s) if m != "%%"]


def effective(md5, lang, human, ai):
    """The cell the runtime will use: human first, then AI, else '' (original)."""
    for table in (human, ai):
        row = table.get(md5)
        if row and row[lang]:
            return row[lang], table is human
    return "", False
