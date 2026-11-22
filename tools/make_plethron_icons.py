#!/usr/bin/env python3 -I
"""Redraws every app icon with the "Plethron" lettering (Plethron Pop, hippodrome-utils/assets/font/pop_subset.ttf) on the existing sky.

The sky and clouds come from tools/macos/AppIcon.png and android/.../ic_launcher_background.png, which stay as they
are apart from the lettering band; this replaces the "Pokémon STADIUM" title inherited from stadion. Writes:
tools/macos/AppIcon.png (1024 px, macOS), tools/launcher/assets/plethron.png (256 px tile, AppImage and launcher),
tools/launcher/assets/plethron.ico (16-256 px, the Windows executable), and the Android launcher icons (plain
ic_launcher.png and the adaptive foreground layer, lettering only). Needs Pillow. Run it once: it repaints the
lettering band of AppIcon.png, so a second run gives the same picture.
Usage: python3 -I tools/make_plethron_icons.py"""
import pathlib
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
FONT = ROOT / "hippodrome-utils/assets/font/pop_subset.ttf"
RES = ROOT / "android/app/src/main/res"
TEXT = "Plethron"
FILL, OUTLINE, SHADOW = (255, 214, 0, 255), (28, 52, 110, 255), (10, 20, 50, 150)


def lettering(size, width):
    """The lettering alone (outline, fill and soft shadow) on a transparent square, `width` px wide."""
    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    font = ImageFont.truetype(str(FONT), 100)
    probe = ImageDraw.Draw(layer).textbbox((0, 0), TEXT, font=font, stroke_width=10)
    font = ImageFont.truetype(str(FONT), max(8, round(100 * width / (probe[2] - probe[0]))))
    stroke = max(1, round(font.size * 0.105))
    box = ImageDraw.Draw(layer).textbbox((0, 0), TEXT, font=font, stroke_width=stroke)
    x = (size - (box[2] - box[0])) // 2 - box[0]
    y = (size - (box[3] - box[1])) // 2 - box[1]
    shadow = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).text((x + size // 170, y + size // 73), TEXT, font=font, fill=SHADOW,
                                stroke_width=stroke + 2, stroke_fill=SHADOW)
    layer = Image.alpha_composite(layer, shadow.filter(ImageFilter.GaussianBlur(max(1, size / 128))))
    ImageDraw.Draw(layer).text((x, y), TEXT, font=font, fill=FILL, stroke_width=stroke, stroke_fill=OUTLINE)
    return layer


def clear_band(im, top, bottom, left, right, sample_x):
    """Paints rows top..bottom between left and right with the sky colour of column sample_x (no clouds there)."""
    px = im.load()
    for y in range(top, bottom):
        c = px[sample_x, y]
        for x in range(left, right):
            px[x, y] = c


# macOS: 1024 px with the system margin; the tile spans 100..924
app = Image.open(ROOT / "tools/macos/AppIcon.png").convert("RGBA")
clear_band(app, 290, 740, 150, 880, 140)
app = Image.alpha_composite(app, lettering(1024, 760))
app.save(ROOT / "tools/macos/AppIcon.png")

# the tile without the margin: AppImage / launcher picture and the Windows .ico
tile = app.crop((100, 100, 924, 924)).resize((256, 256), Image.LANCZOS)
tile.save(ROOT / "tools/launcher/assets/plethron.png")
tile.save(ROOT / "tools/launcher/assets/plethron.ico", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])

# Android: adaptive foreground (lettering inside the 66 dp safe zone of 108 dp) and the plain icon for old versions
for density, layer_px, plain_px in (("mdpi", 108, 48), ("hdpi", 162, 72), ("xhdpi", 216, 96), ("xxhdpi", 324, 144),
                                    ("xxxhdpi", 432, 192)):
    d = RES / f"mipmap-{density}"
    lettering(layer_px, round(layer_px * 0.56)).save(d / "ic_launcher_foreground.png")
    plain = Image.open(d / "ic_launcher_background.png").convert("RGBA").resize((plain_px, plain_px), Image.LANCZOS)
    plain = Image.alpha_composite(plain, lettering(plain_px, round(plain_px * 0.82)))
    mask = Image.new("L", plain.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, plain_px - 1, plain_px - 1), radius=plain_px // 5, fill=255)
    plain.putalpha(mask)
    plain.save(d / "ic_launcher.png")
print("wrote the macOS, Windows, launcher and Android icons")
