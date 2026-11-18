#!/usr/bin/env python3 -I
"""Writes a Japanese Pocket Monsters Red/Green/Blue battery save from scratch, for the routes that need a Game Boy
cartridge in the Transfer Pak (Stadium Zero reads only Japanese carts).

Usage: tools/screen_diff/jp_gen1_save.py <out.sav> [--party SPECIES:LEVEL,...] [--owned RANGES] [--seen RANGES]
                                          [--box SPECIES:LEVEL,...] [--bag ITEM:COUNT,...] [--lab]
RANGES: dex numbers, e.g. 1-8,12-130 (--owned sets owned and seen, --seen sets seen only), on top of the party's.
--lab is the save the Lab routes (party, list, organize, register) use: six in the party, eight in the current
box and a bag with items, so every sub-screen has something to show.
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
BOX_NICKNAMES = ["コノ", "トト", "シシ", "ナナ", "カメ", "ハコ", "メメ", "ノノ", "ヒナ", "サト"]
# Gen 1 item ids (pokered constants)
ITEMS = {"POTION": 0x14, "RARE_CANDY": 0x28, "HP_UP": 0x23, "PROTEIN": 0x24, "PP_UP": 0x4F, "ESCAPE_ROPE": 0x1D,
         "MOON_STONE": 0x0A, "TM06": 0xCE, "HM01": 0xC4}
LAB = dict(party="PIKACHU:25,BULBASAUR:20,CHARMANDER:18,SQUIRTLE:22,PIDGEY:15,RATTATA:12",
           box="PIDGEY:8,RATTATA:9,PIKACHU:11,SQUIRTLE:7,BULBASAUR:6,CHARMANDER:10,RATTATA:30,PIDGEY:33",
           bag="POTION:5,RARE_CANDY:3,HP_UP:2,PROTEIN:2,PP_UP:1,MOON_STONE:1,TM06:1,HM01:1")
BOX_SIZE = 0x566  # count, 31 species bytes, 30 x 33-byte mons, 30 OT names, 30 nicknames (6 bytes each)


def empty_box(sav, at):
    sav[at] = 0
    sav[at + 1] = 0xFF


def write_box(sav, at, mons, ot_id, main_data):
    sav[at] = len(mons)
    sav[at + 1:at + 32] = b"\xFF" * 31
    for i, (species, level) in enumerate(mons):
        data, sid, dex = gb_save.mon(species, level, ot_id)
        sav[at + 1 + i] = sid
        sav[at + 32 + 33 * i:at + 32 + 33 * (i + 1)] = data[:33]
        sav[at + 32 + 990 + 6 * i:at + 32 + 990 + 6 * (i + 1)] = name("サトシ")
        sav[at + 32 + 1170 + 6 * i:at + 32 + 1170 + 6 * (i + 1)] = name(BOX_NICKNAMES[i % len(BOX_NICKNAMES)])
        sav[main_data + (dex - 1) // 8] |= 1 << ((dex - 1) % 8)
        sav[main_data + 0x13 + (dex - 1) // 8] |= 1 << ((dex - 1) % 8)


def parse(spec):
    return [(p.split(":")[0].upper(), int(p.split(":")[1])) for p in spec.split(",") if p]


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
    lab = "--lab" in args
    opt = lambda k, d: args[args.index("--" + k) + 1] if "--" + k in args else (LAB[k] if lab else d)
    party, box, bag = opt("party", "PIKACHU:25,BULBASAUR:20,CHARMANDER:18"), opt("box", ""), opt("bag", "")
    sav = bytearray(0x8000)
    ot_id = 0x3039
    sav[0x2598:0x259E] = name("サトシ")
    main_data = 0x259E
    sav[main_data + 0x53:main_data + 0x59] = name("ナコ")
    sav[main_data + 0x50:main_data + 0x53] = bytes([0x00, 0x30, 0x00])  # money 3000 (BCD)
    sav[main_data + 0x5D:main_data + 0x5F] = ot_id.to_bytes(2, "big")
    items = parse(bag)
    sav[main_data + 0x26] = len(items)  # bag: count, (id, count) pairs, 0xFF
    for i, (item, n) in enumerate(items):
        sav[main_data + 0x27 + 2 * i:main_data + 0x29 + 2 * i] = bytes([ITEMS[item], n])
    sav[main_data + 0x27 + 2 * len(items)] = 0xFF
    sav[main_data + 0x62] = 0x29  # current map: Viridian City Pokémon Center
    sav[0x29ED] |= 0x20  # got the Pokédex
    mons = parse(party)
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
    boxed = parse(box)
    if box:
        write_boxes(sav, boxed, ot_id, main_data)
    sav[0x3594] = (~sum(sav[0x2598:0x3594])) & 0xFF
    open(args[0], "wb").write(bytes(sav))
    print("%s: サトシ, party %s; box %s; bag %s" % (args[0], ", ".join("%s L%d" % m for m in mons),
          ", ".join("%s L%d" % m for m in boxed) or "empty", ", ".join("%s x%d" % m for m in items) or "empty"))


def write_boxes(sav, boxed, ot_id, main_data):
    sav[main_data + 0x2A4] = 0x80  # current box 1, boxes initialised (bit 7)
    write_box(sav, 0x302D, boxed, ot_id, main_data)  # the current box (part of the main data's checksum)
    for bank in (0x4000, 0x6000):  # boxes 1-4 and 5-8 in SRAM banks 2 and 3: empty, with their checksums
        for i in range(4):
            empty_box(sav, bank + BOX_SIZE * i)
        end = bank + BOX_SIZE * 4
        sav[end] = (~sum(sav[bank:end])) & 0xFF
        for i in range(4):
            sav[end + 1 + i] = (~sum(sav[bank + BOX_SIZE * i:bank + BOX_SIZE * (i + 1)])) & 0xFF


if __name__ == "__main__":
    main()
