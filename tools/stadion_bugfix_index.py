#!/usr/bin/env python3
"""Rebuild the case index tables in stadion_bugfix_ref/README.md.

Reads every case file's title, Category and Zero status lines and rewrites the
part of README.md between the BEGIN/END INDEX markers. Run it after adding a
case or changing a status:  python3 tools/stadion_bugfix_index.py
"""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent / "stadion_bugfix_ref"
STATUSES = ["needed", "check", "already ported", "not applicable"]
CATEGORIES = [
    ("rendering", "Rendering"), ("audio", "Audio"), ("abi-64bit", "ABI / 64-bit"),
    ("endianness", "Endianness"), ("input", "Input"), ("saves", "Saves"),
    ("platform-build", "Platform / build"), ("game-logic", "Game logic"),
    ("timing-pacing", "Timing / pacing"), ("performance", "Performance"),
]

def parse(path):
    text = path.read_text(encoding="utf-8")
    title = re.search(r"^# (.+)$", text, re.M).group(1).strip()
    cat = re.search(r"^- \*\*Category:\*\*\s*(\S+)", text, re.M).group(1).strip()
    raw = re.search(r"^- \*\*Zero status:\*\*\s*(.+)$", text, re.M).group(1).strip()
    status = next((s for s in STATUSES if raw.lower().startswith(s)), "check")
    reason = raw[len(status):].lstrip(" .:;,-—()").rstrip(")") if raw.lower().startswith(status) else raw
    return dict(file=path.name, title=title, cat=cat, status=status, reason=reason)

def cell(s):
    return s.replace("|", "\\|")

def table(rows, with_status=True, with_cat=False):
    head = "| Case | " + ("Category | " if with_cat else "") + ("Zero status | " if with_status else "") + "Notes |"
    sep = "|---|" + ("---|" if with_cat else "") + ("---|" if with_status else "") + "---|"
    out = [head, sep]
    for r in rows:
        out.append(f"| [{cell(r['title'])}]({r['file']}) | " + (f"{r['cat']} | " if with_cat else "")
                   + (f"{r['status']} | " if with_status else "") + f"{cell(r['reason'])} |")
    return "\n".join(out)

def main():
    cases = sorted((parse(p) for p in ROOT.glob("*.md") if p.name != "README.md"), key=lambda r: r["file"])
    known = {c for c, _ in CATEGORIES}
    bad = [c["file"] for c in cases if c["cat"] not in known]
    if bad:
        sys.exit("unknown category in: " + ", ".join(bad))
    counts = {s: sum(c["status"] == s for c in cases) for s in STATUSES}
    parts = [f"{len(cases)} cases: " + ", ".join(f"{counts[s]} {s}" for s in STATUSES) + ".", ""]
    parts += ["### Needed in plethron", "", table([c for c in cases if c["status"] == "needed"], False, True), ""]
    parts += ["### Check (could not be decided from the code alone)", "",
              table([c for c in cases if c["status"] == "check"], False, True), ""]
    parts += ["## All cases by category", ""]
    for key, name in CATEGORIES:
        rows = [c for c in cases if c["cat"] == key]
        if rows:
            parts += [f"### {name} ({len(rows)})", "", table(rows), ""]
    readme = ROOT / "README.md"
    text = readme.read_text(encoding="utf-8")
    begin, end = "<!-- BEGIN INDEX -->", "<!-- END INDEX -->"
    i, j = text.index(begin) + len(begin), text.index(end)
    readme.write_text(text[:i] + "\n" + "\n".join(parts) + "\n" + text[j:], encoding="utf-8", newline="\n")
    print(f"{len(cases)} cases indexed")

if __name__ == "__main__":
    main()
