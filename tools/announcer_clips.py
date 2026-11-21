#!/usr/bin/env python3
"""Decodes the announcer's voice lines (JIKKYOU.WBK) from your Zero ROM, to check the subtitles against the audio.

    python3 tools/announcer_clips.py <Zero .z64> <out dir> [--asr <faster-whisper model dir>]

Writes <out>/fxNN.wav for every effect id NN of the announcer bank: the samples the effect plays, in order, as the game
strings them together (effect id NN = the row SUBTITLE_ANNOUNCER_NN of translations/*.csv, see game/src/subtitles.c),
and <out>/effects.txt with each effect's script. With --asr it also transcribes each clip (pip install
faster-whisper; any CTranslate2 Whisper model directory, "small" is enough for these clean voice clips) and writes
<out>/asr.tsv: id, what was heard, the ja column of translations/ai.csv. Lines that differ in meaning are the ones to
listen to. Nothing from the ROM goes into the repository.

How the data is laid out (resident audio init, game/src/audio_commands.c): the audio pak header at ROM 0xB98000 lists
the banks; files[1] is the announcer's: three offsets to the Yay0 effect table (D_800B41A8: count, then at 0x1D0 one
offset per effect), the Yay0 bank (N64 PtrTablesV2: sample descriptors and ADPCM books) and the wave data. An effect
is a small script: 0x81 <n> selects sample n, 0x30 <ticks> plays it for that long (cut off when shorter than the
sample), 0x60 <ticks> waits; numbers >= 0x80 take two bytes. A sample selected and replaced before any 0x30 is never
heard. The clips are mono ADPCM at about 11 kHz (a 0x30 tick is 1/100 s: ~110 samples).
"""

import argparse
import csv
import struct
import sys
import wave
from pathlib import Path

try:
    import crunch64
except ImportError:
    sys.exit("pip install crunch64 (Yay0)")

AUDIO_PAK = 0xB98000
CLIP_RATE = 11025
BANK_MAGIC = b"N64 PtrTablesV2\x00"
REPO = Path(__file__).resolve().parent.parent


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def bank_samples(img):
    """(base, length, book) of each sample of an expanded PtrTablesV2 bank."""
    if img[:16] != BANK_MAGIC:
        raise ValueError("not a PtrTablesV2 bank")
    count, wave_list = u32(img, 0x20), u32(img, 0x2C)
    out = []
    for i in range(count):
        desc = u32(img, wave_list + i * 4)
        base, length, book_off = u32(img, desc) & 0xFFFFFF, u32(img, desc + 4), u32(img, desc + 0x10)
        order, npred = struct.unpack_from(">ii", img, book_off)
        n = min(order * npred * 16, 64)
        out.append((base, length, list(struct.unpack_from(">%dh" % n, img, book_off + 8))))
    return out


def decode_adpcm(book, data):
    """N64 VADPCM, 9-byte frames (same rule as pokestadium's tools/audio/numus_audio.py)."""
    out, v1, v2 = [], 0, 0
    for i in range(0, len(data) // 9 * 9, 9):
        scale, b = data[i] >> 4, (data[i] & 0xF) * 16
        if b + 16 > len(book):
            b = 0
        f1, f2 = book[b + 8], book[b]
        for byte in data[i + 1:i + 9]:
            for nib in (byte >> 4, byte & 0xF):
                cur = ((f1 * v1 + f2 * v2) >> 11) + (((nib ^ 8) - 8) << scale)
                out.append(cur)
                v2, v1 = v1, cur
    return out


def parse_effects(fx):
    """Each effect's script as a list of ('sample', n) / ('play', ticks) / ('wait', ticks)."""
    count = u32(fx, 0)
    offsets = struct.unpack_from(">%dI" % count, fx, 0x1D0)

    def num(p):
        return (((fx[p] & 0x7F) << 8) | fx[p + 1], p + 2) if fx[p] & 0x80 else (fx[p], p + 1)

    effects = []
    for start in offsets:
        p, script = start, []
        while fx[p] != 0x80:
            op = fx[p]
            if op == 0x81:
                v, p = num(p + 1)
                script.append(("sample", v))
            elif op == 0x30:
                v, p = num(p + 1)
                script.append(("play", v))
            elif op == 0x60:
                v, p = num(p + 1)
                script.append(("wait", v))
            elif op in (0xA2, 0xA6):  # one-byte settings
                p += 2
            elif op == 0x84:  # envelope
                p += 8
            else:
                raise ValueError("effect at %#x: unknown op %#x" % (start, op))
        effects.append(script)
    return effects


def render(script, pcms):
    """The clip as heard: each played sample cut to its length in ticks, waits as silence."""
    out, cur = [], None
    per_tick = CLIP_RATE // 100
    for kind, v in script:
        if kind == "sample":
            cur = v
        elif kind == "play" and cur is not None:
            out += pcms[cur][:v * per_tick]
            out += [0] * max(0, v * per_tick - len(pcms[cur]))
        elif kind == "wait" and out:
            out += [0] * (v * per_tick)
    return out


def write_wav(path, pcm):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(CLIP_RATE)
        w.writeframes(struct.pack("<%dh" % len(pcm), *(max(-32768, min(32767, x)) for x in pcm)))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("rom")
    ap.add_argument("out")
    ap.add_argument("--asr", metavar="MODEL", help="faster-whisper model directory: transcribe each clip")
    args = ap.parse_args()

    rom = Path(args.rom).read_bytes()
    pak_files = [u32(rom, AUDIO_PAK + 4 * i) + AUDIO_PAK for i in range(1, 10)]
    ann = pak_files[3]  # num_files, offset1, wave tables, files[0], files[1]
    fx_off, bank_off, wave_off = (u32(rom, ann + 4 * i) + ann for i in range(3))
    fx = crunch64.yay0.decompress(rom[fx_off:bank_off])
    bank = crunch64.yay0.decompress(rom[bank_off:wave_off])
    if bank[0x14:0x1F] != b"JIKKYOU.WBK":
        sys.exit("no JIKKYOU.WBK at %#x: not the Japanese Zero ROM?" % bank_off)
    pcms = [decode_adpcm(book, rom[wave_off + base:wave_off + base + length]) for base, length, book in bank_samples(bank)]
    effects = parse_effects(fx)

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    with open(out / "effects.txt", "w") as f:
        for i, script in enumerate(effects):
            f.write("%02X %s\n" % (i, " ".join("%s:%d" % s for s in script)))
            clip = render(script, pcms)
            if clip:
                write_wav(out / ("fx%02X.wav" % i), clip)
    print("%d effects, %d samples -> %s" % (len(effects), len(pcms), out))

    if args.asr:
        import numpy as np
        from faster_whisper import WhisperModel

        ja = {}
        with open(REPO / "translations/ai.csv", encoding="utf-8", newline="") as f:
            for row in csv.DictReader(f):
                if row["id"].startswith("SUBTITLE_ANNOUNCER_"):
                    ja[row["id"][-2:]] = row["ja"]
        model = WhisperModel(args.asr, device="cpu", compute_type="int8")
        with open(out / "asr.tsv", "w", encoding="utf-8") as f:
            f.write("id\theard\tja\n")
            for i, script in enumerate(effects):
                clip = render(script, pcms)
                if not clip:
                    continue
                a = np.array(clip, dtype=np.float32) / 32768
                a = np.interp(np.linspace(0, len(a) - 1, len(a) * 16000 // CLIP_RATE), np.arange(len(a)), a)
                segs, _ = model.transcribe(a.astype(np.float32), language="ja", beam_size=5)
                heard = "".join(s.text for s in segs).strip()
                f.write("%02X\t%s\t%s\n" % (i, heard, ja.get("%02X" % i, "")))
                print("%02X  %s\n    %s" % (i, heard, ja.get("%02X" % i, "")), flush=True)


if __name__ == "__main__":
    main()
