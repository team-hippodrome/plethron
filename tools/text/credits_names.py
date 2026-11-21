#!/usr/bin/env python3
"""Fill the staff credits' name cells in translations/ai.csv from YOUR ROM's kana.

The staff roll (BattleText_DrawTextBox, game/src/fragments/battle/menu_panels.c) draws
each name in kana as family name + space + given name. In the translated languages the
port draws the row's whole translation instead, split at its first space the same way.

  Latin languages (en es de pt fr it)  given name first, as Nintendo's English credits
      do: the spelling from Nintendo's own English credits of Pokémon Stadium (US, 1999)
      and Pokémon Stadium 2 (US, 2001) where the person is in them (OFFICIAL below; where
      those credits spell one person two ways, the form without a doubled vowel), else
      Hepburn without macrons or doubled long vowels (Hideki Okawa, Koichi Hasebe).
      Benimaru Itoh keeps the spelling he publishes under.
  Korean (ko)  Hangul in the Japanese order (family name first), per the Korean
      standard for Japanese (외래어 표기법): 미야모토 시게루, 이와타 사토루.
  Japanese (ja)  stays empty: the original kana.

The Latin spellings are a fixed table, checked against the kana (they must romanize to
the same sounds); the Hangul is generated from the kana. No kana is written anywhere.

Usage:  python3 tools/text/credits_names.py path/to/rom.z64 [--check]
        --check  change nothing, exit 1 if a cell differs from what would be written
"""
import argparse
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
import romtext  # noqa: E402
import tables  # noqa: E402
from romanize_names import to_hiragana  # noqa: E402

LATIN = ["en", "es", "de", "pt", "fr", "it"]

# The staff roll: {char* str; u8 color; pad[3]} entries (D_82189D8C), color 1 = a heading, "おわり" ends a page.
TABLE_VRAM = 0x82189D8C
TABLE_SIZE = 2704
VRAM_TO_ROM = 0x81F81300  # the battle fragment's link address minus its ROM offset

# id -> the Latin-script name (given family). Every credits name row is listed.
OFFICIAL = {
    "CREDITS_HIROSHI_YAMAUCHI": "Hiroshi Yamauchi",
    "CREDITS_SHIGERU_MIYAMOTO": "Shigeru Miyamoto",
    "CREDITS_KENJI_MIKI": "Kenji Miki",
    "CREDITS_TSUNEKAZU_ISHIHARA": "Tsunekazu Ishihara",
    "CREDITS_SATORU_IWATA": "Satoru Iwata",
    "CREDITS_TAKAO_SHIMIZU": "Takao Shimizu",
    "CREDITS_TATSUYA_HISHIDA": "Tatsuya Hishida",
    "CREDITS_KENSUKE_TANABE": "Kensuke Tanabe",
    "CREDITS_YASUNARI_NISHIDA": "Yasunari Nishida",
    "CREDITS_KENJI_YAMAMOTO": "Kenji Yamamoto",
    "CREDITS_YASUHIRO_KAWAGUCHI": "Yasuhiro Kawaguchi",
    "CREDITS_TOMOHIRO_KAWASE": "Tomohiro Kawase",
    "CREDITS_YUZURU_OGAWA": "Yuzuru Ogawa",
    "CREDITS_SHUNSAKU_KITAMURA": "Shunsaku Kitamura",
    "CREDITS_MASAFUMI_KAWAMURA": "Masafumi Kawamura",
    "CREDITS_HIDEAKI_SHIMIZU": "Hideaki Shimizu",
    "CREDITS_KENTA_NAGATA": "Kenta Nagata",
    "CREDITS_TORU_MINEGISHI": "Toru Minegishi",
    "CREDITS_MITSUHIRO_HIKINO": "Mitsuhiro Hikino",
    "CREDITS_SHINYA_TAKAHASHI": "Shinya Takahashi",
    "CREDITS_MAKOTO_WADA": "Makoto Wada",
    "CREDITS_TSUYOSHI_WATANABE": "Tsuyoshi Watanabe",
    "CREDITS_TOKIHIKO_TOYODA": "Tokihiko Toyoda",
    "CREDITS_HIROAKI_TAKENAKA": "Hiroaki Takenaka",
    "CREDITS_HIDEKI_OOKAWA": "Hideki Okawa",
    "CREDITS_NORIKO_IKEGAWA": "Noriko Ikegawa",
    "CREDITS_TOMOYOSHI_YAMANE": "Tomoyoshi Yamane",
    "CREDITS_SHINYA_SANO": "Shinya Sano",
    "CREDITS_RYUUJI_KOBAYASHI": "Ryuji Kobayashi",
    "CREDITS_AKIKO_SHIBATA": "Akiko Shibata",
    "CREDITS_AKIO_SHIBUTANI": "Akio Shibutani",
    "CREDITS_HIROYUKI_FUJIWARA": "Hiroyuki Fujiwara",
    "CREDITS_KEI_HOSHIBA": "Kei Hoshiba",
    "CREDITS_KENJI_MATSUURA": "Kenji Matsuura",
    "CREDITS_JUNPEI_MIZUSAKI": "Junpei Mizusaki",
    "CREDITS_HIDEO_YAMATO": "Hideo Yamato",
    "CREDITS_BENIMARU_ITOH": "Benimaru Itoh",
    "CREDITS_TOMOAKI_IMAKUNI": "Tomoaki Imakuni",
    "CREDITS_SHIZU_HIGASHIYAMA": "Shizu Higashiyama",
    "CREDITS_TAKESHI_IIZUKA": "Takeshi Iizuka",
    "CREDITS_HAJIME_KUROYANAGI": "Hajime Kuroyanagi",
    "CREDITS_HITOSHI_KOBAYASHI": "Hitoshi Kobayashi",
    "CREDITS_AKIRA_YAMAZAKI": "Akira Yamazaki",
    "CREDITS_TOORU_KURASHIGE": "Toru Kurashige",
    "CREDITS_HIDEKI_ONO": "Hideki Ono",
    "CREDITS_MASATO_NAGASHIMA": "Masato Nagashima",
    "CREDITS_KEN_IKUJI": "Ken Ikuji",
    "CREDITS_HIROAKI_ITOU": "Hiroaki Ito",
    "CREDITS_MASAYA_KANOU": "Masaya Kano",
    "CREDITS_AKIRA_MATSUMOTO": "Akira Matsumoto",
    "CREDITS_SHINICHI_YOSHIKAWA": "Shinichi Yoshikawa",
    "CREDITS_KAZUO_YAZAWA": "Kazuo Yazawa",
    "CREDITS_MITSUTERU_SATOU": "Mitsuteru Sato",
    "CREDITS_TAKEMI_ICHIKAWA": "Takemi Ichikawa",
    "CREDITS_ATSUKO_UJIIE": "Atsuko Ujiie",
    "CREDITS_SATOMI_KOKUBO": "Satomi Kokubo",
    "CREDITS_RYUUTA_KUSUMI": "Ryuta Kusumi",
    "CREDITS_NACHI_SUGIYAMA": "Nachi Sugiyama",
    "CREDITS_HIKARU_JINGUUJI": "Hikaru Jinguji",
    "CREDITS_HO_WEE_HWA": "Ho Wee Hwa",
    "CREDITS_TERUTAKA_IWASAKI": "Terutaka Iwasaki",
    "CREDITS_TOMOMI_TAKEDA": "Tomomi Takeda",
    "CREDITS_HIROSHI_ISHIKAWA": "Hiroshi Ishikawa",
    "CREDITS_YOUICHI_KOTABE": "Yoichi Kotabe",
    "CREDITS_YASUSHI_MATSUBARA": "Yasushi Matsubara",
    "CREDITS_TOMOAKI_NAKAHARA": "Tomoaki Nakahara",
    "CREDITS_JUNJI_MORII": "Junji Morii",
    "CREDITS_SHIGEFUMI_HINO": "Shigefumi Hino",
    "CREDITS_MASANAO_ARIMOTO": "Masanao Arimoto",
    "CREDITS_JUN_YAMAZAKI": "Jun Yamazaki",
    "CREDITS_CHIHARU_ITOU": "Chiharu Ito",
    "CREDITS_HIROHITO_YOSHIMOTO": "Hirohito Yoshimoto",
    "CREDITS_YOSHINORI_TANIMOTO": "Yoshinori Tanimoto",
    "CREDITS_SATOSHI_TAJIRI": "Satoshi Tajiri",
    "CREDITS_KEN_SUGIMORI": "Ken Sugimori",
    "CREDITS_SHIGEKI_MORIMOTO": "Shigeki Morimoto",
    "CREDITS_MOTOFUMI_FUJIWARA": "Motofumi Fujiwara",
    "CREDITS_ATSUKO_NISHIDA": "Atsuko Nishida",
    "CREDITS_TAKENORI_OOTA": "Takenori Ota",
    "CREDITS_JUNICHI_MASUDA": "Junichi Masuda",
    "CREDITS_KOUJI_NISHINO": "Koji Nishino",
    "CREDITS_HIROYUKI_JINNAI": "Hiroyuki Jinnai",
    "CREDITS_TAKASHI_SAKUMA": "Takashi Sakuma",
    "CREDITS_YOSHIYUKI_TAGAWA": "Yoshiyuki Tagawa",
    "CREDITS_SHIGEZO_KAWASE": "Shigezo Kawase",
    "CREDITS_HIRONOBU_KAKUI": "Hironobu Kakui",
    "CREDITS_SHIN_HASEGAWA": "Shin Hasegawa",
    "CREDITS_KIMIYOSHI_FUKUI": "Kimiyoshi Fukui",
    "CREDITS_KEIZOU_KATOU": "Keizo Kato",
    "CREDITS_KOUICHI_HASEBE": "Koichi Hasebe",
    "CREDITS_KAZUO_KOUSHIMA": "Kazuo Koshima",
    "CREDITS_KAZUNORI_KOSHIISHI": "Kazunori Koshiishi",
    "CREDITS_YUUJI_KIMIJIMA": "Yuji Kimijima",
    "CREDITS_KENICHIROU_ASHIDA": "Kenichiro Ashida",
    "CREDITS_KOUJI_MITSUNARI": "Koji Mitsunari",
    "CREDITS_KATSUHITO_YAMAUCHI": "Katsuhito Yamauchi",
    "CREDITS_TADASHI_INOUE": "Tadashi Inoue",
    "CREDITS_YASUHIRO_SAKAI": "Yasuhiro Sakai",
    "CREDITS_WATARU_YAMAGUCHI": "Wataru Yamaguchi",
    "CREDITS_YUUSUKE_NAKANO": "Yusuke Nakano",
    "CREDITS_MASANORI_SATOU": "Masanori Sato",
    "CREDITS_ATSUSHI_TEJIMA": "Atsushi Tejima",
    "CREDITS_NARUHISA_KAWANO": "Naruhisa Kawano",
    "CREDITS_MICHIYO_OOSHIO": "Michiyo Oshio",
}

# A Chinese name (ほー うぃーほわ): written family name first in English too, and its Hangul follows the Chinese
# syllables (Wee Hwa = 위화) rather than the kana's ほわ.
FAMILY_FIRST = {"CREDITS_HO_WEE_HWA"}
KO_OVERRIDE = {"CREDITS_HO_WEE_HWA": "호 위화"}

# --- kana -> Hangul (외래어 표기법, Japanese) ---------------------------------------------------------------------------
# Jamo indices of the syllable-block formula: onsets, vowels, codas.
L = {c: i for i, c in enumerate("ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ")}
V = {c: i for i, c in enumerate("ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ")}
T = {"": 0, "ㄴ": 4, "ㅅ": 19}
VOWELS = {"a": "ㅏ", "i": "ㅣ", "u": "ㅜ", "e": "ㅔ", "o": "ㅗ"}
YVOWELS = {"a": "ㅑ", "u": "ㅠ", "o": "ㅛ"}
# kana -> (initial onset, medial onset, vowel letter)
ROWS = {
    "あいうえお": ("ㅇ", "ㅇ"), "かきくけこ": ("ㄱ", "ㅋ"), "がぎぐげご": ("ㄱ", "ㄱ"),
    "さしすせそ": ("ㅅ", "ㅅ"), "ざじずぜぞ": ("ㅈ", "ㅈ"), "たちつてと": ("ㄷ", "ㅌ"),
    "だぢづでど": ("ㄷ", "ㄷ"), "なにぬねの": ("ㄴ", "ㄴ"), "はひふへほ": ("ㅎ", "ㅎ"),
    "ばびぶべぼ": ("ㅂ", "ㅂ"), "ぱぴぷぺぽ": ("ㅍ", "ㅍ"), "まみむめも": ("ㅁ", "ㅁ"),
    "らりるれろ": ("ㄹ", "ㄹ"),
}
KANA = {}
for row, (ini, med) in ROWS.items():
    for k, v in zip(row, "aiueo"):
        KANA[k] = (ini, med, VOWELS[v])
KANA.update({"や": ("ㅇ", "ㅇ", "ㅑ"), "ゆ": ("ㅇ", "ㅇ", "ㅠ"), "よ": ("ㅇ", "ㅇ", "ㅛ"), "わ": ("ㅇ", "ㅇ", "ㅘ"),
             "を": ("ㅇ", "ㅇ", "ㅗ")})
# し じ ち ぢ: ㅅ/ㅈ/ㅈ(ㅊ) + ㅣ; す ず つ づ: ㅡ; つ is always ㅆ
KANA["ち"] = ("ㅈ", "ㅊ", "ㅣ")
KANA["ぢ"] = ("ㅈ", "ㅈ", "ㅣ")
for k, o in (("す", "ㅅ"), ("ず", "ㅈ"), ("づ", "ㅈ")):
    KANA[k] = (o, o, "ㅡ")
KANA["つ"] = ("ㅆ", "ㅆ", "ㅡ")
# Long vowels are not written: う after an o/u syllable, お after an o syllable, ー (not before え or お: いのうえ).
LONG_AFTER = {"う": ("ㅗ", "ㅜ", "ㅛ", "ㅠ"), "お": ("ㅗ", "ㅛ")}


def _block(onset, vowel, coda=""):
    return chr(0xAC00 + (L[onset] * 21 + V[vowel]) * 28 + T[coda])


def hangul_word(kana):
    s = to_hiragana(kana)
    out = []  # [onset, vowel, coda]
    i = 0
    while i < len(s):
        c = s[i]
        nxt = s[i + 1] if i + 1 < len(s) else ""
        if c == "ん" and out:
            out[-1][2] = "ㄴ"
        elif c == "っ" and out:
            out[-1][2] = "ㅅ"
        elif c == "ー" or (c in LONG_AFTER and out and out[-1][1] in LONG_AFTER[c] and not out[-1][2]
                            and not (nxt and nxt in "えお")):  # いのうえ is i-no-u-e: 이노우에
            pass
        elif c == "う" and nxt == "ぃ":
            out.append(["ㅇ", "ㅟ", ""])
            i += 1
        elif c in KANA:
            ini, med, vowel = KANA[c]
            onset = ini if not out else med
            if nxt and nxt in "ゃゅょ":
                y = YVOWELS["auo"["ゃゅょ".index(nxt)]]
                if c in "じちぢ":  # じゃ 자, ちゅ 주/추 (しゃ keeps its y: 샤)
                    y = {"ㅑ": "ㅏ", "ㅠ": "ㅜ", "ㅛ": "ㅗ"}[y]
                out.append([onset, y, ""])
                i += 1
            else:
                out.append([onset, vowel, ""])
        else:
            raise ValueError("no Hangul for %r in %r" % (c, kana))
        i += 1
    return "".join(_block(*b) for b in out)


def hangul_name(kana):
    return " ".join(hangul_word(w) for w in kana.split(" "))


# --- Latin check: the official spelling must say the same sounds as the kana ----------------------------------------
def _sounds(latin):
    s = latin.lower().replace(" ", "")
    for a, b in (("ou", "o"), ("oo", "o"), ("oh", "o"), ("uu", "u"), ("ee", "i"), ("wee", "ui")):
        s = s.replace(a, b)
    return s


def staff_names(rom):
    """[(kana, md5)] of the staff roll's name lines (not headings, not page ends, not blanks)."""
    off = TABLE_VRAM - VRAM_TO_ROM
    out = []
    for e in range(off, off + TABLE_SIZE, 8):
        ptr, color = struct.unpack(">IB", rom[e:e + 5])
        if ptr < 0x80000000:
            break
        o = ptr - VRAM_TO_ROM
        raw = rom[o:rom.index(b"\0", o)]
        kana = raw.decode("euc-jp")
        if color == 1 or " " not in kana.strip():
            continue
        out.append((kana, hashlib.md5(raw).hexdigest()))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("rom")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    rom, _sha1, ok = romtext.load_rom(args.rom)
    if not ok:
        sys.exit("not the supported ROM (sha1 %s)" % romtext.SUPPORTED_SHA1)
    ai, problems = tables.read("ai")
    if problems:
        sys.exit("\n".join(problems))
    md5_ids = tables.by_md5(ai)
    seen = set()
    diffs = 0
    for kana, md5 in staff_names(rom):
        ids = [i for i in md5_ids.get(md5, []) if i.startswith("CREDITS_")]
        if len(ids) != 1:
            sys.exit("no single CREDITS_ row for a staff name (md5 %s)" % md5)
        rid = ids[0]
        if rid in seen:
            continue
        seen.add(rid)
        if rid not in OFFICIAL:
            sys.exit("%s: add its Latin spelling to OFFICIAL" % rid)
        latin = OFFICIAL[rid]
        if rid not in FAMILY_FIRST:
            given, family = latin.rsplit(" ", 1)
            fam_kana, given_kana = kana.split(" ", 1)
            want_sounds = _sounds(romanize_kana(fam_kana) + romanize_kana(given_kana))
            if _sounds(family + given) != want_sounds:
                sys.exit("%s: %r does not match the kana (%s)" % (rid, latin, want_sounds))
        want = {lang: latin for lang in LATIN}
        want["ko"] = KO_OVERRIDE.get(rid) or hangul_name(kana)
        row = ai[rid]
        for lang, value in want.items():
            if row[lang] != value:
                diffs += 1
                print("%s %s: %r -> %r" % (rid, lang, row[lang], value))
                row[lang] = value
    missing = set(OFFICIAL) - seen
    if missing:
        sys.exit("OFFICIAL names not in the staff roll: %s" % ", ".join(sorted(missing)))
    if args.check:
        sys.exit(1 if diffs else 0)
    if diffs:
        tables.write("ai", ai.values())
    print("%d staff names, %d cells changed" % (len(seen), diffs))


def romanize_kana(kana):
    from romanize_names import romanize_line

    return romanize_line(kana)


if __name__ == "__main__":
    main()
