#!/usr/bin/env python3 -I
"""Copies a Stadium Zero SRAM save (32 KiB, N64 byte order) and changes its options record, for the routes that need
something unlocked.

Usage: tools/screen_diff/zero_sram_options.py <in.sra> <out.sra> [--gb-frames 0-2]

<in.sra> is any save the game wrote: the port's (build/saves/stadium.sra, or <out>/port.sra after a screen_diff run
without @sram) or ares's (tools/ares_ref with ARES_REF_SRAM=<32 KiB of 0xFF> and ARES_REF_SRAM_OUT=<file>, run to the
main menu: the game formats the blank chip). --gb-frames sets options +0x02, the GB Player tier that Master Ball Cup
clears raise (Save_SetGbPlayerFrameTier): 1 unlocks the Doduo picture frame and speed x2, 2 adds the Dodrio frame and
speed x4 (Z cycles frames, C-Right the speed). Both copies of the record get the new value and their "POKE" footer
sum (a byte sum of everything before it, big-endian).
"""
import sys

OPTIONS, SIZE = 0x7A40, 0x10


def fix(sav, off):
    sav[off + SIZE - 2:off + SIZE] = (sum(sav[off:off + SIZE - 2]) & 0xFFFF).to_bytes(2, "big")


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        sys.exit(__doc__)
    sav = bytearray(open(args[0], "rb").read())
    if len(sav) != 0x8000 or sav[OPTIONS + SIZE - 6:OPTIONS + SIZE - 2] != b"POKE":
        sys.exit("%s: not a Stadium Zero SRAM save in N64 byte order (no options record)" % args[0])
    if "--gb-frames" in args:
        tier = int(args[args.index("--gb-frames") + 1])
        if not 0 <= tier <= 2:
            sys.exit("--gb-frames takes 0, 1 or 2")
        for off in (OPTIONS, OPTIONS + SIZE):  # primary, backup
            sav[off + 2] = tier
            fix(sav, off)
    open(args[1], "wb").write(bytes(sav))
    print("%s: options %s" % (args[1], sav[OPTIONS:OPTIONS + SIZE].hex()))


if __name__ == "__main__":
    main()
