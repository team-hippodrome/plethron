#!/usr/bin/env python3
"""Draws the switch menu's status symbols for the translated languages and writes them as C data.

Usage: tools/font/make_status_symbols.py [<out.h>] [--preview <png>]

The battle switch menu (and the roster panel drawn the same way) marks a fainted or statused Pokémon with a 24x11
stamp of kana (ひんし, ねむり, どく, やけど, こおり, まひ; the pointers D_8218AFDC[1..6]), which only the Japanese modes can
read. The other languages draw these symbols instead (game/src/type_symbols.c): a stamp in the status colour with a
white pictogram (crossed-out eyes, a moon with "z", bubbles, a flame, a snowflake, a bolt). Poison, burn, freeze and
paralysis reuse the pictograms of the move panel's type symbols (tools/font/make_type_symbols.py), so a status reads
like the type that causes it. Everything is drawn from simple shapes here, nothing is taken from any game.

The stamps are drawn at 8x and scaled down to 48x22 texels, twice the original texture's size, so they stay sharp at
the port's 2x resolution and up; the game draws them over the original 24x11 rectangle. They are squarer than the
pill-shaped type symbols so the two never look alike.

The output is RGBA16 (5-5-5-1) in N64 byte order, like the textures the game loads from the ROM. It is the port's own
art (no ROM data), so the generated header is committed: src/port/generated/status_symbols.h.
"""
import importlib.util, os, sys

from PIL import Image, ImageDraw, ImageFilter

W, H = 48, 22  # texels
SS = 8  # supersampling
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

_spec = importlib.util.spec_from_file_location("make_type_symbols", os.path.join(HERE, "make_type_symbols.py"))
types = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(types)

# Index of D_8218AFDC (LabPC_StatusToIconIndex; 1 = fainted, set by the callers when HP is 0) -> name, stamp colour.
STATUSES = {
    1: ("fainted", (92, 92, 104)),
    2: ("sleep", (108, 120, 176)),
    3: ("poison", (148, 72, 176)),
    4: ("burn", (206, 52, 44)),
    5: ("freeze", (72, 172, 204)),
    6: ("paralysis", (214, 170, 16)),
}
COUNT = 7
# status -> the type symbol whose pictogram it borrows
BORROWED = {"poison": "poison", "burn": "fire", "freeze": "ice", "paralysis": "electric"}


def pictogram(name, d, cx, cy, u, fill):
    """Draws `name`'s symbol centred on (cx, cy); `u` is one unit of a 16x16 design grid (x may run wider)."""
    if name in BORROWED:
        types.pictogram(BORROWED[name], d, cx, cy, u, fill)
        return

    def P(x, y):
        return (cx + (x - 8) * u, cy + (y - 8) * u)

    def line(pts, w):
        d.line([P(x, y) for x, y in pts], fill=fill, width=int(w * u), joint="curve")
        for x, y in (pts[0], pts[-1]):
            r = w * u / 2
            px, py = P(x, y)
            d.ellipse([px - r, py - r, px + r, py + r], fill=fill)

    def disc(x, y, r, f):
        px, py = P(x, y)
        d.ellipse([px - r * u, py - r * u, px + r * u, py + r * u], fill=f)

    if name == "fainted":  # a knocked-out face: two crossed-out eyes over a wavy mouth
        for ex in (1.8, 14.2):
            line([(ex - 2.6, 2.4), (ex + 2.6, 7.6)], 2.2)
            line([(ex - 2.6, 7.6), (ex + 2.6, 2.4)], 2.2)
        line([(3.5, 13.2), (5.75, 11.4), (8, 13.2), (10.25, 11.4), (12.5, 13.2)], 1.8)
    elif name == "sleep":  # a crescent moon with a "z"
        disc(4.5, 8, 7, fill)
        disc(7.6, 5.6, 6, 0)
        line([(9.5, 3.5), (15, 3.5), (9.5, 10.5), (15, 10.5)], 2)
        line([(16.6, 10.6), (19.6, 10.6), (16.6, 14.6), (19.6, 14.6)], 1.5)
    else:
        raise ValueError(name)


def symbol(name, colour):
    big = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    r = 4 * SS
    dark = tuple(int(c * 0.5) for c in colour)
    light = tuple(min(255, int(c + (255 - c) * 0.35)) for c in colour)
    # the stamp: dark rim, the colour, a lighter upper half
    d.rounded_rectangle([0.5 * SS, 0.5 * SS, W * SS - 0.5 * SS, H * SS - 0.5 * SS], radius=r, fill=dark + (255,))
    d.rounded_rectangle([2 * SS, 2 * SS, W * SS - 2 * SS, H * SS - 2 * SS], radius=r - SS, fill=colour + (255,))
    d.rounded_rectangle([3 * SS, 2.5 * SS, W * SS - 3 * SS, H * SS / 2], radius=r - 2 * SS, fill=light + (255,))
    # the pictogram, with a dark outline so it reads on every colour
    mask = Image.new("L", big.size, 0)
    pictogram(name, ImageDraw.Draw(mask), W * SS / 2, H * SS / 2, 1.12 * SS, 255)
    outline = mask.filter(ImageFilter.MaxFilter(2 * SS + 1))
    big.paste(dark + (255,), (0, 0), outline)
    big.paste((255, 255, 255, 255), (0, 0), mask)
    return big.resize((W, H), Image.LANCZOS)


def rgba16(img):
    px = img.convert("RGBA").tobytes()
    out = bytearray()
    for i in range(0, W * H * 4, 4):
        r, g, b, a = px[i:i + 4]
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
    out = args[0] if args else os.path.join(ROOT, "src", "port", "generated", "status_symbols.h")
    imgs = {s: symbol(n, c) for s, (n, c) in STATUSES.items()}
    lines = ["// Generated by tools/font/make_status_symbols.py: the port's own status symbols (no ROM data). Do not edit.",
             "// RGBA16 in N64 byte order, %dx%d texels each, indexed like D_8218AFDC (NULL: no status)." % (W, H),
             "#define STATUS_SYMBOL_WIDTH %d" % W, "#define STATUS_SYMBOL_HEIGHT %d" % H,
             "#define STATUS_SYMBOL_COUNT %d" % COUNT, ""]
    for s, (n, _) in sorted(STATUSES.items()):
        data = rgba16(imgs[s])
        lines.append("static const u8 sStatusSymbol_%s[] __attribute__((aligned(8))) = {" % n)
        for i in range(0, len(data), 24):
            lines.append("    " + ", ".join("0x%02X" % b for b in data[i:i + 24]) + ",")
        lines.append("};")
    lines.append("static const u8* const sStatusSymbols[STATUS_SYMBOL_COUNT] = {")
    for s in range(COUNT):
        lines.append("    %s, // %d" % (("sStatusSymbol_" + STATUSES[s][0]) if s in STATUSES else "NULL", s))
    lines.append("};")
    open(out, "w").write("\n".join(lines) + "\n")
    if preview:
        sheet = Image.new("RGBA", (W * 3 * 6 + 40, H * 2 * 6 + 30), (16, 24, 150, 255))
        for k, s in enumerate(sorted(STATUSES)):
            im = imgs[s].resize((W * 6, H * 6), Image.NEAREST)
            sheet.alpha_composite(im, (10 + (k % 3) * (W * 6 + 10), 10 + (k // 3) * (H * 6 + 10)))
        sheet.save(preview)


if __name__ == "__main__":
    main()
