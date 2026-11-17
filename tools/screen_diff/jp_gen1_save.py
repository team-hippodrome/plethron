#!/usr/bin/env python3 -I
"""Writes a Japanese Pocket Monsters Red/Green/Blue battery save from scratch, for the routes that need a Game Boy
cartridge in the Transfer Pak (Stadium Zero reads only Japanese carts).

Usage: tools/screen_diff/jp_gen1_save.py <out.sav> [--party SPECIES:LEVEL,...] [--owned RANGES] [--seen RANGES]
RANGES: dex numbers, e.g. 1-8,12-130 (--owned sets owned and seen, --seen sets seen only), on top of the party's.
Species: PIKACHU BULBASAUR CHARMANDER SQUIRTLE PIDGEY RATTATA (tools/gb_save.py's table).

What Zero checks is set: player name サトシ, rival ナコ, nicknames ハナ, ヒメ, カコ, ..., the party with its Pokédex owned/seen flags, the got-Pokédex
event flag (else ずかん is greyed out) and a Pokémon Center as the current map (else 「ポケモンセンターにいない」), with
the main data checksum. Everything else stays zero: the Game Boy game itself would not continue from it, which the
GB Player route does not need (it stops at the Game Boy title screen). Offsets: the Japanese games' save routine
(main data 0x259E, party 0x2ED5, checksum 0x3594).
"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import gb_save  # SPECIES, mon()

KANA = {"カ": 0x85, "コ": 0x89, "サ": 0x8A, "シ": 0x8B, "ト": 0x93, "ナ": 0x94, "ノ": 0x98, "ハ": 0x99, "ヒ": 0x9A,
        "メ": 0xA0}  # (the kana confirmed on screen so far)
NICKNAMES = ["ハナ", "ヒメ", "カコ", "ノコ", "ナシ", "メカ"]


def name(s):
    b = bytes(KANA[c] for c in s) + b"\x50"
    return b + b"\x50" * (6 - len(b))


def parse_ranges(text):
    out = []
    for part in text.split(","):
        lo, _, hi = part.partition("-")
        out += range(int(lo), int(hi or lo) + 1)
    if any(not 1 <= d <= 151 for d in out):
        sys.exit("dex numbers are 1-151: %s" % text)
    return out


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    party = args[args.index("--party") + 1] if "--party" in args else "PIKACHU:25,BULBASAUR:20,CHARMANDER:18"
    sav = bytearray(0x8000)
    ot_id = 0x3039
    sav[0x2598:0x259E] = name("サトシ")
    main_data = 0x259E
    sav[main_data + 0x53:main_data + 0x59] = name("ナコ")
    sav[main_data + 0x50:main_data + 0x53] = bytes([0x00, 0x30, 0x00])  # money 3000 (BCD)
    sav[main_data + 0x5D:main_data + 0x5F] = ot_id.to_bytes(2, "big")
    sav[main_data + 0x26] = 0  # bag: empty
    sav[main_data + 0x27] = 0xFF
    sav[main_data + 0x62] = 0x29  # current map: Viridian City Pokémon Center
    sav[0x29ED] |= 0x20  # got the Pokédex
    mons = [(p.split(":")[0].upper(), int(p.split(":")[1])) for p in party.split(",") if p]
    base = 0x2ED5
    sav[base] = len(mons)
    sav[base + 1:base + 8] = b"\xFF" * 7
    for i, (species, level) in enumerate(mons):
        data, sid, dex = gb_save.mon(species, level, ot_id)
        sav[base + 1 + i] = sid
        sav[base + 8 + 44 * i:base + 8 + 44 * (i + 1)] = data
        sav[base + 8 + 264 + 6 * i:base + 8 + 264 + 6 * (i + 1)] = name("サトシ")
        sav[base + 8 + 300 + 6 * i:base + 8 + 300 + 6 * (i + 1)] = name(NICKNAMES[i])
        sav[main_data + (dex - 1) // 8] |= 1 << ((dex - 1) % 8)  # owned
        sav[main_data + 0x13 + (dex - 1) // 8] |= 1 << ((dex - 1) % 8)  # seen
    for flag, offsets in (("--owned", (0, 0x13)), ("--seen", (0x13,))):
        if flag in args:
            for dex in parse_ranges(args[args.index(flag) + 1]):
                for off in offsets:
                    sav[main_data + off + (dex - 1) // 8] |= 1 << ((dex - 1) % 8)
    sav[0x3594] = (~sum(sav[0x2598:0x3594])) & 0xFF
    open(args[0], "wb").write(bytes(sav))
    print("%s: サトシ, party %s" % (args[0], ", ".join("%s L%d" % m for m in mons)))


if __name__ == "__main__":
    main()
