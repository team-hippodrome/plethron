"""Read and write translations/ai.csv and translations/human.csv."""
import csv
import os
import re

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
TDIR = os.path.join(ROOT, "translations")
LANGS = ["ja", "en", "es", "de", "pt", "ko"]
HEADER = ["md5"] + LANGS + ["note"]
MD5_RE = re.compile(r"^[0-9a-f]{32}$")
NAME_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")

# Identical bytes that mean different things in different places. Such a string
# gets one named row per meaning instead of an md5 row; the port binds each name
# to the ROM table entry it comes from (src/port/text_lookup.cpp).
# md5 of the shared bytes -> {name: (ROM offset, where it comes from)}
SPLIT = {
    "8e926012b0747ce3776e6b3dec9f35a8": {
        "HAUNTER_NAME": (0x7AE40, "species table, Haunter (#93) name"),
        "GHOST_TYPE_NAME": (0x7B410, "type name table, Ghost"),
    },
    "ff41602f16393b9ff989839e04f284e7": {
        "POISON_PIN_CATEGORY": (0x7A8C0, "species table, category of Nidoran F, Nidorina, Nidoran M, Nidorino (0x7A8C0, 0x7A8D8, 0x7A904, 0x7A91C)"),
        "POISON_STING_MOVE_NAME": (0x7B664, "move name table, Poison Sting"),
    },
    "d66215b1e9db23506f9b670a4a8610ec": {
        "POISON_STATUS_NAME": (0x7B3A8, "status label list, poisoned"),
        "POISON_TYPE_NAME": (0x7B3E8, "type name table, Poison"),
    },
    "c1ff32329ef23f70d464f0fbdba746ee": {
        "FREEZE_STATUS_NAME": (0x7B3B8, "status label list, frozen"),
        "ICE_TYPE_NAME": (0x7B448, "type name table, Ice"),
    },
}
NAME_SOURCE = {n: m for m, names in SPLIT.items() for n in names}
# printf conversions the game uses (%s, %d, %3d, %02d, %c, %%, %1$s ...)
SPEC_RE = re.compile(r"%(?:\d+\$)?[-+ 0#]*\d*(?:\.\d+)?[hl]*[diouxXcsf%]")


def path(name):
    return os.path.join(TDIR, name + ".csv")


def read(name):
    """Return (ordered dict key -> row dict, list of problems).

    A key is an md5, or a name from SPLIT for strings whose bytes are shared."""
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
            key = row["md5"]
            if NAME_RE.match(key):
                if key not in NAME_SOURCE:
                    problems.append("%s.csv row %d: name %s is not in tables.SPLIT" % (name, n, key))
                    continue
            elif not MD5_RE.match(key):
                problems.append("%s.csv row %d: bad key %r" % (name, n, key))
                continue
            elif key in SPLIT:
                problems.append("%s.csv row %d: %s has shared bytes; use the names %s instead"
                                % (name, n, key, ", ".join(SPLIT[key])))
                continue
            if row["md5"] in rows:
                problems.append("%s.csv row %d: duplicate key %s" % (name, n, row["md5"]))
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


def keys_for(md5):
    """The table keys a ROM string is looked up by: its md5, or its SPLIT names."""
    return list(SPLIT[md5]) if md5 in SPLIT else [md5]


def effective(key, lang, human, ai):
    """The cell the runtime will use: human first, then AI, else '' (original)."""
    for table in (human, ai):
        row = table.get(key)
        if row and row[lang]:
            return row[lang], table is human
    return "", False
