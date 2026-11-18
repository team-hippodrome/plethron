#!/usr/bin/env python3 -I
"""Draws the battle move panel's type symbols for the translated languages and writes them as C data.

Usage: tools/font/make_type_symbols.py [<out.h>] [--preview <png>]

Zero labels each move in the battle move panel (hold R) with its type as a 32x9 texture of kana (でんき, ノーマル, ...),
which only the Japanese modes can read. The other languages draw these symbols instead (src/port/type_symbols.c): a
pill in the type's colour with a white pictogram, made from simple shapes here (circle, flame, drop, leaf, bolt ...)
rather than taken from any game. They are drawn at 8x and scaled down to 64x18 texels, twice the original texture's
size, so they stay sharp at the port's 2x resolution and up; the game draws them over the original 32x9 rectangle.

The output is RGBA16 (5-5-5-1) in N64 byte order, like the textures the game loads from the ROM. It is the port's own
art (no ROM data), so the generated header is committed: src/port/generated/type_symbols.h.
"""
import math, os, sys

from PIL import Image, ImageDraw, ImageFilter

W, H = 64, 18  # texels
SS = 8  # supersampling
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Gen 1 type ids (the index of gMoveData[].type) -> name, pill colour.
TYPES = {
    0x00: ("normal", (168, 160, 144)),
    0x01: ("fighting", (190, 52, 40)),
    0x02: ("flying", (128, 144, 232)),
    0x03: ("poison", (156, 64, 160)),
    0x04: ("ground", (200, 164, 88)),
    0x05: ("rock", (164, 140, 60)),
    0x07: ("bug", (140, 168, 32)),
    0x08: ("ghost", (100, 84, 150)),
    0x14: ("fire", (236, 120, 40)),
    0x15: ("water", (72, 132, 232)),
    0x16: ("grass", (84, 180, 72)),
    0x17: ("electric", (232, 188, 24)),
    0x18: ("psychic", (232, 84, 132)),
    0x19: ("ice", (96, 196, 208)),
    0x1A: ("dragon", (100, 72, 216)),
}
COUNT = 0x1B


def pictogram(name, d, cx, cy, u, fill):
    """Draws `name`'s symbol centred on (cx, cy); `u` is one unit of a 16x16 design grid."""
    def P(x, y):
        return (cx + (x - 8) * u, cy + (y - 8) * u)

    def poly(pts):
        d.polygon([P(x, y) for x, y in pts], fill=fill)

    def line(pts, w, f=None):  # f=0 cuts the line out of what is drawn
        f = fill if f is None else f
        d.line([P(x, y) for x, y in pts], fill=f, width=int(w * u), joint="curve")
        for x, y in (pts[0], pts[-1]):
            r = w * u / 2
            px, py = P(x, y)
            d.ellipse([px - r, py - r, px + r, py + r], fill=f)

    def disc(x, y, r, f=None):
        f = fill if f is None else f
        px, py = P(x, y)
        d.ellipse([px - r * u, py - r * u, px + r * u, py + r * u], fill=f)

    def ring(x, y, r, w):
        px, py = P(x, y)
        d.ellipse([px - r * u, py - r * u, px + r * u, py + r * u], outline=fill, width=int(w * u))

    if name == "normal":  # a plain ring
        ring(8, 8, 6, 2.6)
    elif name == "fighting":  # an impact burst
        pts = []
        for i in range(16):
            a = math.pi * 2 * i / 16 - math.pi / 2
            r = 7.6 if i % 2 == 0 else 3.6
            pts.append((8 + r * math.cos(a), 8 + r * math.sin(a)))
        poly(pts)
    elif name == "flying":  # a wing: three feathers sweeping back
        for i, (ln, y) in enumerate(((13, 4.5), (10.5, 8), (8, 11.5))):
            line([(2, 3.5 + i * 1.2), (2 + ln * 0.55, y - 0.5), (2 + ln, y)], 2.4)
    elif name == "poison":  # bubbles rising
        disc(6, 10.5, 4.2)
        disc(11.5, 5.5, 2.6)
        disc(13, 11.5, 1.6)
    elif name == "ground":  # dunes over a ground line
        poly([(0.5, 12), (4.5, 5.5), (7.5, 9), (11, 3), (15.5, 12)])
        line([(0.5, 14.3), (15.5, 14.3)], 1.8)
    elif name == "rock":  # a faceted boulder
        poly([(2.5, 14), (1, 8), (4.5, 2.5), (11, 2), (15, 7.5), (13.5, 14)])
        line([(4.5, 2.5), (7, 7.5), (15, 7.5)], 1.1, 0)
        line([(7, 7.5), (6, 14)], 1.1, 0)
    elif name == "bug":  # a beetle: body, head, antennae
        disc(8, 10, 4.6)
        disc(8, 4.6, 2.4)
        line([(7, 3.4), (4.2, 0.8)], 1.3)
        line([(9, 3.4), (11.8, 0.8)], 1.3)
    elif name == "ghost":  # a sheet with a wavy hem
        pts = [(3, 15), (3, 7)]
        for i in range(0, 181, 15):
            a = math.radians(180 + i)
            pts.append((8 + 5 * math.cos(a), 7 + 5.5 * math.sin(a)))
        pts += [(13, 7), (13, 15), (11.3, 12.8), (9.7, 15), (8, 12.8), (6.3, 15), (4.7, 12.8)]
        poly(pts)
    elif name == "fire":  # a flame
        disc(8, 10.6, 4.8)
        poly([(3.4, 9.5), (6, 4), (7.4, 6), (8.6, 0.6), (11.2, 5.6), (12.6, 9.5)])
    elif name == "water":  # a drop
        disc(8, 10.4, 4.8)
        poly([(3.6, 8.8), (8, 0.8), (12.4, 8.8)])
    elif name == "grass":  # a leaf with its midrib cut out
        leaf = []
        for i in range(0, 181, 10):
            t = math.radians(i)
            leaf.append((8 + 7 * math.cos(t) * 0.92, 8 - 4.2 * math.sin(t)))
        for i in range(180, 361, 10):
            t = math.radians(i)
            leaf.append((8 + 7 * math.cos(t) * 0.92, 8 - 4.2 * math.sin(t)))
        rot = []
        for x, y in leaf:
            a = math.radians(-35)
            dx, dy = x - 8, y - 8
            rot.append((8 + dx * math.cos(a) - dy * math.sin(a), 8 + dx * math.sin(a) + dy * math.cos(a)))
        poly(rot)
    elif name == "electric":  # a bolt
        poly([(10.5, 0.5), (3.5, 9), (7.6, 9), (5.5, 15.5), (12.5, 6.8), (8.4, 6.8)])
    elif name == "psychic":  # an eye
        top = [(x, 8 - 5 * math.sin(math.pi * (x - 0.5) / 15)) for x in [0.5 + i * 0.5 for i in range(31)]]
        poly(top + [(x, 16 - y) for x, y in reversed(top)])
        disc(8, 8, 3.6, 0)
        disc(8, 8, 2.3)
    elif name == "ice":  # a six-armed snowflake
        for i in range(3):
            a = math.pi * i / 3 + math.pi / 2
            dx, dy = 6.8 * math.cos(a), 6.8 * math.sin(a)
            line([(8 - dx, 8 - dy), (8 + dx, 8 + dy)], 1.9)
        disc(8, 8, 2)
    elif name == "dragon":  # a horned crest: a crescent with a spine
        poly([(2, 14), (5.5, 1.5), (7.2, 8.5), (8.8, 8.5), (10.5, 1.5), (14, 14), (8, 10.5)])
    else:
        raise ValueError(name)


def symbol(name, colour):
    big = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    r = H * SS / 2 - 0.5 * SS
    dark = tuple(int(c * 0.55) for c in colour)
    light = tuple(min(255, int(c + (255 - c) * 0.35)) for c in colour)
    # the pill: dark rim, the colour, a lighter upper half
    d.rounded_rectangle([0.5 * SS, 0.5 * SS, W * SS - 0.5 * SS, H * SS - 0.5 * SS], radius=r, fill=dark + (255,))
    d.rounded_rectangle([1.5 * SS, 1.5 * SS, W * SS - 1.5 * SS, H * SS - 1.5 * SS], radius=r - SS, fill=colour + (255,))
    d.rounded_rectangle([2.5 * SS, 2 * SS, W * SS - 2.5 * SS, H * SS / 2], radius=(H * SS / 2 - 2 * SS) / 2,
                        fill=light + (255,))
    # the pictogram, with a dark outline so it reads on every colour
    mask = Image.new("L", big.size, 0)
    pictogram(name, ImageDraw.Draw(mask), W * SS / 2, H * SS / 2, 0.86 * SS, 255)
    outline = mask.filter(ImageFilter.MaxFilter(2 * SS + 1))
    big.paste(dark + (255,), (0, 0), outline)
    big.paste((255, 255, 255, 255), (0, 0), mask)
    return big.resize((W, H), Image.LANCZOS)


def rgba16(img):
    out = bytearray()
    for r, g, b, a in list(img.convert("RGBA").tobytes()[i:i + 4] for i in range(0, W * H * 4, 4)):
        v = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (1 if a >= 128 else 0)
        out += bytes(((v >> 8) & 0xFF, v & 0xFF))
    return out


def main():
    args = sys.argv[1:]
    preview = None
    if "--preview" in args:
        i = args.index("--preview")
        preview = args[i + 1]
        del args[i:i + 2]
    out = args[0] if args else os.path.join(ROOT, "src", "port", "generated", "type_symbols.h")
    imgs = {t: symbol(n, c) for t, (n, c) in TYPES.items()}
    lines = ["// Generated by tools/font/make_type_symbols.py: the port's own type symbols (no ROM data). Do not edit.",
             "// RGBA16 in N64 byte order, %dx%d texels each, indexed by Gen 1 type id (NULL: no such type)." % (W, H),
             "#define TYPE_SYMBOL_WIDTH %d" % W, "#define TYPE_SYMBOL_HEIGHT %d" % H, "#define TYPE_SYMBOL_COUNT %d" % COUNT,
             ""]
    for t, (n, _) in sorted(TYPES.items()):
        data = rgba16(imgs[t])
        lines.append("static const u8 sTypeSymbol_%s[] __attribute__((aligned(8))) = {" % n)
        for i in range(0, len(data), 24):
            lines.append("    " + ", ".join("0x%02X" % b for b in data[i:i + 24]) + ",")
        lines.append("};")
    lines.append("static const u8* const sTypeSymbols[TYPE_SYMBOL_COUNT] = {")
    for t in range(COUNT):
        lines.append("    %s, // 0x%02X" % (("sTypeSymbol_" + TYPES[t][0]) if t in TYPES else "NULL", t))
    lines.append("};")
    open(out, "w").write("\n".join(lines) + "\n")
    if preview:
        sheet = Image.new("RGBA", (W * 5 * 4 + 40, H * 3 * 4 + 30), (40, 40, 90, 255))
        for k, t in enumerate(sorted(TYPES)):
            im = imgs[t].resize((W * 4, H * 4), Image.NEAREST)
            sheet.alpha_composite(im, (10 + (k % 5) * (W * 4 + 4), 10 + (k // 5) * (H * 4 + 4)))
        sheet.save(preview)


if __name__ == "__main__":
    main()
