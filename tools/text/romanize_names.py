#!/usr/bin/env python3
"""Fill the transliterated (romaji) cells of the name rows from YOUR ROM's kana.

The port can show COM Pokémon nicknames and the Nintendo Cup '97 finalists' names two
ways (Options > Names, port setting gPort.NameStyle):

  transliterated  the kana romanized, e.g. おれさま -> Oresama   (translations/ai.csv)
  localized       the meaning translated, e.g. おれさま -> THE MAN  (translations/localized.csv)

The rows in localized.csv are the name rows. For each of them this script reads the
kana from the ROM, romanizes it (Hepburn without macrons, long vowels doubled, the
project's convention in pokestadium0 docs/glossary.md) and writes it to the en, es,
de, pt and ko cells of the same id in ai.csv. ja stays empty (the original kana). It
also refreshes localized.csv's kana column, which holds the name in kana so a
translation can be discussed next to it (names only; nothing else from the ROM is
ever written to a committed file).

Usage:  python3 tools/text/romanize_names.py path/to/rom.z64 [--check]
        --check  change nothing, exit 1 if a cell differs from what would be written
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import romtext  # noqa: E402
import tables  # noqa: E402

LATIN = ["en", "es", "de", "pt", "ko", "fr", "it"]

# Romaji of each kana (hiragana; katakana is folded to hiragana first).
BASE = {
    "あ": "a", "い": "i", "う": "u", "え": "e", "お": "o",
    "か": "ka", "き": "ki", "く": "ku", "け": "ke", "こ": "ko",
    "が": "ga", "ぎ": "gi", "ぐ": "gu", "げ": "ge", "ご": "go",
    "さ": "sa", "し": "shi", "す": "su", "せ": "se", "そ": "so",
    "ざ": "za", "じ": "ji", "ず": "zu", "ぜ": "ze", "ぞ": "zo",
    "た": "ta", "ち": "chi", "つ": "tsu", "て": "te", "と": "to",
    "だ": "da", "ぢ": "ji", "づ": "zu", "で": "de", "ど": "do",
    "な": "na", "に": "ni", "ぬ": "nu", "ね": "ne", "の": "no",
    "は": "ha", "ひ": "hi", "ふ": "fu", "へ": "he", "ほ": "ho",
    "ば": "ba", "び": "bi", "ぶ": "bu", "べ": "be", "ぼ": "bo",
    "ぱ": "pa", "ぴ": "pi", "ぷ": "pu", "ぺ": "pe", "ぽ": "po",
    "ま": "ma", "み": "mi", "む": "mu", "め": "me", "も": "mo",
    "や": "ya", "ゆ": "yu", "よ": "yo",
    "ら": "ra", "り": "ri", "る": "ru", "れ": "re", "ろ": "ro",
    "わ": "wa", "ゐ": "i", "ゑ": "e", "を": "o", "ん": "n", "ゔ": "vu",
}
SMALL_Y = {"ゃ": "a", "ゅ": "u", "ょ": "o"}
SMALL_V = {"ぁ": "a", "ぃ": "i", "ぅ": "u", "ぇ": "e", "ぉ": "o"}
# Syllable + small vowel (katakana loan sounds): ウィ wi, シェ she, フィ fi, ティ ti ...
LOAN = {
    ("う", "ぃ"): "wi", ("う", "ぇ"): "we", ("う", "ぉ"): "wo",
    ("ゔ", "ぁ"): "va", ("ゔ", "ぃ"): "vi", ("ゔ", "ぇ"): "ve", ("ゔ", "ぉ"): "vo",
    ("し", "ぇ"): "she", ("じ", "ぇ"): "je", ("ち", "ぇ"): "che",
    ("て", "ぃ"): "ti", ("で", "ぃ"): "di", ("と", "ぅ"): "tu", ("ど", "ぅ"): "du",
    ("ふ", "ぁ"): "fa", ("ふ", "ぃ"): "fi", ("ふ", "ぇ"): "fe", ("ふ", "ぉ"): "fo",
    ("つ", "ぁ"): "tsa", ("つ", "ぃ"): "tsi", ("つ", "ぇ"): "tse", ("つ", "ぉ"): "tso",
}
WIDE = {"　": " ", "Ａ": "A", "Ｂ": "B", "１": "1", "２": "2"}


def to_hiragana(s):
    return "".join(chr(ord(c) - 0x60) if "ァ" <= c <= "ヶ" else c for c in s)


# Honorific / pet-name endings, written after a hyphen as in the localized names (Jura-sama, Shawa-don). おれさま is
# one word ("my royal self"), not おれ + さま.
SUFFIXES = ["さま", "さん", "くん", "ちゃん", "はん", "やん", "どん"]
ONE_WORD = {"おれさま"}


def romanize_line(line):
    s = to_hiragana(line)
    for suf in SUFFIXES:
        if s.endswith(suf) and len(s) - len(suf) >= 2 and s not in ONE_WORD and s != suf + suf:  # サンサン: a doubling
            stem = romanize_line(line[: len(line) - len(suf)])
            return stem + "-" + romanize_line(suf).lower()
    out = []
    i = 0
    double_next = False
    while i < len(s):
        c = s[i]
        nxt = s[i + 1] if i + 1 < len(s) else ""
        if c == "っ":
            double_next = True
            i += 1
            continue
        if c == "ー":  # long mark: repeat the last vowel
            last = next((ch for ch in reversed("".join(out)) if ch in "aeiou"), "")
            out.append(last)
            i += 1
            continue
        if c in WIDE:
            out.append(WIDE[c])
            i += 1
            continue
        if c in BASE and nxt in SMALL_Y and c not in "あいうえおん":
            stem = BASE[c][:-1]  # き -> k, し -> sh, ち -> ch, じ -> j
            syl = (stem if stem.endswith(("sh", "ch", "j")) else stem + "y") + SMALL_Y[nxt]
            i += 2
        elif (c, nxt) in LOAN:
            syl = LOAN[(c, nxt)]
            i += 2
        elif c in BASE and nxt in SMALL_V and BASE[c][-1] == SMALL_V[nxt]:
            syl = BASE[c] + SMALL_V[nxt]  # げぇ -> gee, がぁ -> gaa (a drawn-out vowel)
            i += 2
        elif c in BASE:
            syl = BASE[c]
            i += 1
        elif c in SMALL_V:
            syl = SMALL_V[c]
            i += 1
        else:
            raise ValueError("no romaji for %r in %r" % (c, line))
        if double_next:
            syl = ("t" + syl) if syl.startswith("ch") else (syl[0] + syl)
            double_next = False
        out.append(syl)
    text = "".join(out)
    return text[:1].upper() + text[1:]


def romanize(kana):
    return "\n".join(romanize_line(line) for line in kana.split("\n"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("rom")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()

    rom, sha1, ok = romtext.load_rom(a.rom)
    if not ok:
        sys.exit("ROM sha1 %s is not the supported dump %s" % (sha1, romtext.SUPPORTED_SHA1))
    found = romtext.extract(rom)
    ai, p1 = tables.read("ai")
    loc, p2 = tables.read("localized")
    if p1 or p2:
        sys.exit("\n".join(p1 + p2))

    changes = 0
    for key, lrow in loc.items():
        row = ai.get(key)
        if row is None or row["md5"] != lrow["md5"]:
            sys.exit("localized.csv %s: no ai.csv row with the same md5" % key)
        if row["md5"] not in found:
            sys.exit("localized.csv %s: md5 not in the ROM" % key)
        kana = romtext.to_cell(found[row["md5"]][2].decode("euc-jp"))
        roma = romanize(kana)
        if lrow["kana"] != kana:
            changes += 1
            lrow["kana"] = kana
        for lang in LATIN:
            if row[lang] != roma:
                changes += 1
                if a.check:
                    print("%s %s: %r, romaji is %r" % (key, lang, row[lang], roma))
                row[lang] = roma
    if a.check:
        print("%d cell(s) differ" % changes)
        sys.exit(1 if changes else 0)
    tables.write("ai", ai.values())
    tables.write("localized", loc.values())
    print("%d cell(s) updated" % changes)


if __name__ == "__main__":
    main()
