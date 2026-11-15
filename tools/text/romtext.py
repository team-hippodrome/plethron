"""Find the game's text in a user-supplied Pokémon Stadium (Japan) ROM.

Shared by the kana viewer, the CSV checker and the coverage report. Nothing here
writes ROM bytes into the repository: callers either print md5 keys or write into
translations/rom-text/, which is gitignored.

Keys match the runtime text hook: lowercase hex md5 of the string's raw bytes as the
game holds them in RAM, up to but not including the NUL. The game's text is
EUC-JP (bytes >= 0x80 start a two-byte JIS character, < 0x80 is ASCII incl. '\\n').
"""
import hashlib
import re
import struct

SUPPORTED_SHA1 = "8bc8d2ff7df25b26bd0c353d2adfe83e4e3a7a87"

# Uncompressed text lives in the resident image and the fragment images below this
# address. Past it the ROM is models, textures, audio and Yay0 archives, where a raw
# byte scan only finds noise.
RAW_TEXT_END = 0x2B3000

# Yay0 archives that hold text (COM trainer classes, names, party nicknames).
# Found by decompressing all ~790 Yay0 blocks (run show_kana.py --all-blobs to redo it).
TEXT_BLOBS = (0x3FFD78,)
# A blob counts as a text table only with at least this many hits; lone hits in
# texture/model blobs are noise.
BLOB_MIN_HITS = 10

FULLWIDTH_EXTRA = "〜・ー…×→←↑↓♂♀＿"
PRINTF_RE = re.compile(r"%[-+ 0#]*\d*(?:\.\d+)?[hl]*[diouxXcsf]")


def load_rom(path):
    """Read a ROM, normalise it to big-endian .z64 order, return (bytes, sha1, ok)."""
    data = bytearray(open(path, "rb").read())
    magic = bytes(data[:4])
    if magic == b"\x37\x80\x40\x12":  # .v64, byte-swapped halfwords
        data[0::2], data[1::2] = data[1::2], data[0::2]
    elif magic == b"\x40\x12\x37\x80":  # .n64, little-endian words
        for i in range(0, len(data) - 3, 4):
            data[i:i + 4] = data[i:i + 4][::-1]
    data = bytes(data)
    sha1 = hashlib.sha1(data).hexdigest()
    return data, sha1, sha1 == SUPPORTED_SHA1


def yay0_decompress(data, off):
    if data[off:off + 4] != b"Yay0":
        raise ValueError("no Yay0 header at 0x%X" % off)
    size, link, chunk = struct.unpack(">III", data[off + 4:off + 16])
    mask_pos, link, chunk = off + 16, off + link, off + chunk
    out = bytearray()
    bits = mask = 0
    while len(out) < size:
        if bits == 0:
            mask = struct.unpack(">I", data[mask_pos:mask_pos + 4])[0]
            mask_pos += 4
            bits = 32
        if mask & 0x80000000:
            out.append(data[chunk])
            chunk += 1
        else:
            v = struct.unpack(">H", data[link:link + 2])[0]
            link += 2
            dist = (v & 0xFFF) + 1
            n = v >> 12
            if n == 0:
                n = data[chunk] + 18
                chunk += 1
            else:
                n += 2
            for _ in range(n):
                out.append(out[-dist])
        mask = (mask << 1) & 0xFFFFFFFF
        bits -= 1
    return bytes(out)


def _is_kana(o):
    return 0x3040 <= o <= 0x30FF


def judge(raw):
    """Return the decoded string if raw looks like game text, else None."""
    try:
        u = raw.decode("euc-jp")
    except UnicodeDecodeError:
        return None
    if not u:
        return None
    good = kana = fw = kanji = 0
    for ch in u:
        o = ord(ch)
        if _is_kana(o):
            kana += 1
            good += 1
        elif 0xFF01 <= o <= 0xFF5E or 0x3000 <= o <= 0x303F or ch in FULLWIDTH_EXTRA:
            fw += 1
            good += 1
        elif o < 0x80:
            good += 1
        elif 0x4E00 <= o <= 0x9FFF:
            kanji += 1
    if good / len(u) < 0.8:
        return None
    nonascii = sum(1 for c in u if ord(c) >= 0x80)
    if kana >= 2:
        if len(set(u)) <= 2 and len(u) > 4:  # 'いいいいい' style fill patterns
            return None
        return u
    if kanji == 0 and nonascii >= 1 and PRINTF_RE.search(u):  # '%sの%s!', 'Ｌ%d', '×%02d'
        return u
    if kana == 0 and kanji == 0 and fw >= 2 and fw == nonascii == len(u.replace(" ", "")):
        return u  # 'ＣＯＭ', '？？？？？'
    return None


def scan(data):
    """Yield (offset, raw_bytes) for every NUL-terminated text string in data."""
    pos = 0
    for seg in data.split(b"\x00"):
        n = len(seg)
        if n >= 2:
            # Keep the longest tail of the segment that parses as EUC-JP text.
            i = start = 0
            while i < n:
                c = seg[i]
                if 0x20 <= c < 0x7F or c == 0x0A:
                    i += 1
                elif 0xA1 <= c <= 0xFE and i + 1 < n and 0xA1 <= seg[i + 1] <= 0xFE:
                    i += 2
                else:
                    i += 1
                    start = i
            raw = seg[start:]
            off = pos + start
            # Name fields in party records start word-aligned; a stray ASCII-range
            # byte just before one (a stat byte) is not part of the name.
            if off % 4 == 3 and len(raw) > 1 and raw[0] < 0x80 and raw[1] >= 0x80:
                raw = raw[1:]
                off += 1
            # A stray kanji-looking byte pair in front of a word-aligned string is
            # the tail of the previous field (e.g. '屡' before a credits title).
            if off % 4 == 2 and len(raw) > 2 and raw[0] >= 0x80 and raw[2] >= 0x80:
                try:
                    lead = raw[:2].decode("euc-jp")
                except UnicodeDecodeError:
                    lead = ""
                if lead and 0x4E00 <= ord(lead) <= 0x9FFF and not (0x4E00 <= ord(raw[2:4].decode("euc-jp", "replace")[:1] or "a") <= 0x9FFF):
                    raw = raw[2:]
                    off += 2
            if judge(raw) is not None:
                yield off, raw
        pos += n + 1


def md5(raw):
    return hashlib.md5(raw).hexdigest()


def extract(rom, all_blobs=False):
    """Return an ordered dict md5 -> (source, offset, raw) for every unique string.

    source is 'rom' for raw ROM text or 'yay0@XXXXXX' for text inside a Yay0 archive
    (offset is then relative to the decompressed archive).
    """
    found = {}

    def add(src, off, raw):
        found.setdefault(md5(raw), (src, off, raw))

    for off, raw in scan(rom[:RAW_TEXT_END]):
        add("rom", off, raw)
    if all_blobs:
        starts, i = [], rom.find(b"Yay0")
        while i >= 0:
            starts.append(i)
            i = rom.find(b"Yay0", i + 1)
    else:
        starts = TEXT_BLOBS
    for b in starts:
        try:
            blob = yay0_decompress(rom, b)
        except (ValueError, IndexError, struct.error):
            continue
        hits = list(scan(blob))
        if len(hits) < BLOB_MIN_HITS:
            continue
        for off, raw in hits:
            add("yay0@%06X" % b, off, raw)
    return found


def to_cell(u):
    """ROM string -> CSV cell text: real newlines, other control bytes as \\xNN."""
    return "".join(c if c == "\n" or ord(c) >= 0x20 else "\\x%02X" % ord(c) for c in u)
