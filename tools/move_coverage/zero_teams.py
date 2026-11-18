#!/usr/bin/env python3 -I
"""Writes a Stadium Zero SRAM save (32 KiB, N64 byte order) whose とうろく (registered teams) records hold the given
teams, for tools/move_coverage: Free Battle lists registered teams on its entry screen, so 1P and the COM can enter
any team without a Game Boy cartridge.

Record layout (docs/n64-save-format.md in the decomp's project files): kind 16, 16 teams at 0x250 * n, each copy
0x128 bytes, primary then backup; u16 flags (bit 0 registered), trainer name (GB charset, 0x50-terminated, 6 bytes),
u16 trainer ID, 8 bytes second name, u16 count, then per Pokémon the Gen 1 box struct (0x21 bytes), nickname (6) and
OT name (6); the last 6 bytes of each copy are "POKE" and a u16 byte sum of everything before it. The rest of the
save stays blank: the game formats the records it finds invalid and keeps these.

Used as a module (tools/move_coverage/run.py); the species data comes from the moveset dump (PORT_DUMP_MOVESETS),
which the port reads from the user's ROM: nothing ROM-derived is stored in the repository.
"""
import math

TEAM_BASE, TEAM_STRIDE, TEAM_COPY = 0x0000, 0x250, 0x128
# Gen 1 Japanese character set: ア (0x80) to ノ (0x98) in gojūon order (カ 0x85, サ 0x8A, ト 0x93, ナ 0x94, ノ 0x98 as
# tools/screen_diff/jp_gen1_save.py confirmed on screen)
KATAKANA = "アイウエオカキクケコサシスセソタチツテトナニヌネノ"
GB_CHAR = {c: 0x80 + i for i, c in enumerate(KATAKANA)}


def gb_text(s, n=6):
    b = bytes(GB_CHAR[c] for c in s)[: n - 1] + b"\x50"
    return b + b"\x50" * (n - len(b))


def code_name(i, letters=2):
    """A short unique katakana name for index i (アア, アイ, ...)."""
    out = ""
    for _ in range(letters):
        out = KATAKANA[i % len(KATAKANA)] + out
        i //= len(KATAKANA)
    return out


def exp_for(level, growth):
    n = level
    if growth == 0:
        return n ** 3
    if growth == 3:
        return max(0, int(6 * n ** 3 / 5 - 15 * n * n + 100 * n - 140))
    if growth == 4:
        return int(4 * n ** 3 / 5)
    if growth == 5:
        return int(5 * n ** 3 / 4)
    return n ** 3


def stat(base, dv, statexp, level, hp=False):
    v = ((base + dv) * 2 + int(math.ceil(math.sqrt(statexp))) // 4) * level // 100
    return v + level + 10 if hp else v + 5


def box_struct(species, level, moves, pps, ot_id, dvs=(8, 8, 8, 8), statexp=0, pp_ups=0):
    """species: dict from the moveset dump (gen1, hp, type1, type2, catch, growth); moves: up to 4 move ids; pps: their
    base PP; statexp: one value or five (HP, Attack, Defense, Speed, Special). -> the 0x21-byte Gen 1 box struct."""
    atk, dfn, spd, spc = dvs
    se = list(statexp) if isinstance(statexp, (list, tuple)) else [statexp] * 5  # HP, Attack, Defense, Speed, Special
    hpdv = ((atk & 1) << 3) | ((dfn & 1) << 2) | ((spd & 1) << 1) | (spc & 1)
    hp = stat(species["hp"], hpdv, se[0], level, True)
    b = bytearray(0x21)
    b[0] = species["gen1"]
    b[1:3] = hp.to_bytes(2, "big")
    b[3] = level
    b[4] = 0
    b[5], b[6] = species["type1"], species["type2"]
    b[7] = species["catch"]
    for i, m in enumerate(moves[:4]):
        b[8 + i] = m
    b[0x0C:0x0E] = ot_id.to_bytes(2, "big")
    b[0x0E:0x11] = exp_for(level, species["growth"]).to_bytes(3, "big")
    for i in range(5):
        b[0x11 + 2 * i:0x13 + 2 * i] = se[i].to_bytes(2, "big")
    b[0x1B] = (atk << 4) | dfn
    b[0x1C] = (spd << 4) | spc
    for i, pp in enumerate(pps[:4]):
        full = pp + (pp // 5) * pp_ups
        b[0x1D + i] = (pp_ups << 6) | min(full, 63)
    return bytes(b)


def team_record(trainer, ot_id, mons):
    """mons: [(box_struct bytes, nickname)] -> one 0x128-byte copy with its footer."""
    r = bytearray(TEAM_COPY)
    r[0:2] = (1).to_bytes(2, "big")
    r[2:8] = gb_text(trainer)
    r[8:10] = ot_id.to_bytes(2, "big")
    r[0x0A:0x12] = b"\x50" * 8
    r[0x12:0x14] = len(mons).to_bytes(2, "big")
    for i, (data, nick) in enumerate(mons):
        at = 0x14 + 0x2D * i
        r[at:at + 0x21] = data
        r[at + 0x21:at + 0x27] = gb_text(nick)
        r[at + 0x27:at + 0x2D] = gb_text(trainer)
    r[TEAM_COPY - 6:TEAM_COPY - 2] = b"POKE"
    r[TEAM_COPY - 2:] = (sum(r[:TEAM_COPY - 2]) & 0xFFFF).to_bytes(2, "big")
    return bytes(r)


def write_sram(path, teams):
    """teams: up to 16 (trainer name, trainer id, mons) in registration slot order."""
    assert len(teams) <= 16
    sav = bytearray(0x8000)
    for n, (trainer, ot_id, mons) in enumerate(teams):
        rec = team_record(trainer, ot_id, mons)
        at = TEAM_BASE + TEAM_STRIDE * n
        sav[at:at + TEAM_COPY] = rec
        sav[at + TEAM_COPY:at + 2 * TEAM_COPY] = rec
    open(path, "wb").write(bytes(sav))
