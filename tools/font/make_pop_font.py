#!/usr/bin/env python3
"""Writes assets/font/pop_subset.ttf: the prototype "Plethron Pop" font for translated text, an option next to the
default GNU Unifont glyphs (Port_Font in src/port/unicode_font.cpp, the "Text font" setting).

Goal (Julius, 2026-11-18): something with the 90s "cool but cartoony" feel of Stadium Zero's own kana font instead of
Unifont's plain pixel shapes, monospaced so the port's layout and label fitting behave the same. The runtime gives
every glyph the game font's white fill, one-pixel dark outline and two-cycle shading, so this file only holds shapes:

- Latin (ASCII, Latin-1 for es/de/pt, the punctuation and symbols the tables use): drawn here, an original design.
  Each glyph is a skeleton of strokes (lines and elliptical arcs on a 500 x 1000 cell) that is stroked with round caps
  and joins, which gives the heavy rounded-geometric look of the game's own alphabet. Half width (500 units), like the
  Unifont glyphs it replaces, so text measures the same.
- Kana, kanji, CJK punctuation and fullwidth forms: M PLUS Rounded 1c ExtraBold (SIL OFL 1.1), scaled into the cell.
- Hangul: Jua (SIL OFL 1.1), a rounded cartoony Korean face, centred in a full-width (1000 unit) cell.

    python3 tools/font/make_pop_font.py --mplus MPLUSRounded1c-ExtraBold.ttf --jua Jua-Regular.ttf
    python3 tools/font/make_pop_font.py --preview out.png      # also renders a specimen of the Latin design

Both base fonts come from https://github.com/google/fonts (ofl/mplusrounded1c, ofl/jua).

--full writes a complete version for a type designer to edit and for the other Stadium games' translations (not used
by the port): Latin Extended-A too, every kana, kanji and CJK symbol M+ has, the kanji M+ lacks from Zen Maru Gothic
Black (ofl/zenmarugothic) and all 11,172 Hangul syllables, the ones Jua lacks from Nanum Gothic ExtraBold
(ofl/nanumgothic) with rounded corners. Both are SIL OFL 1.1 too; Nanum's Reserved Font Names (Nanum, NanumGothic, ...)
are not used. The character set is the one
make_unifont_subset.py collects from the translation tables. No ROM data goes into this file.
The output is a Modified Version of both base fonts under the SIL OFL 1.1 (assets/font/LICENSE-pop.txt); neither
declares a Reserved Font Name.
"""
import argparse
import math
import os
import sys

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.basePen import BasePen
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont
from shapely.geometry import LineString, MultiPolygon, Point, Polygon
from shapely.ops import unary_union

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_unifont_subset import ROOT, wanted_chars  # noqa: E402

OUT = os.path.join(ROOT, "assets", "font", "pop_subset.ttf")

# ---- metrics (units per em 1000; the em is the whole cell the runtime draws, outline included) ----
UPM = 1000
ASCENT, DESCENT = 850, 150  # baseline at 85% of the cell, like the game font's row 14 of 16
HALF, FULL = 500, 1000

SW = 112  # stroke width of the Latin design
H = SW / 2
L, R, MID = 95, 405, 250  # stroke centre lines of the left side, right side and middle
BASE = H  # centre line of strokes sitting on the baseline
CAP = 610 - H  # cap height 610
XH = 460 - H  # x-height 460 (big, like the game's lowercase)
ASC = 640 - H  # ascenders a little over the caps
DSC = -125 + H  # descenders
CMID = (BASE + CAP) / 2
XMID = (BASE + XH) / 2


# ---- skeleton helpers: every helper returns a list of shapely geometries (centre lines and dots) ----
def seg(*pts):
    return [LineString(pts)]


def arc_pts(cx, cy, rx, ry, a0, a1, n=None):
    n = n or max(6, int(abs(a1 - a0) / 7))
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def arc(cx, cy, rx, ry, a0, a1):
    return [LineString(arc_pts(cx, cy, rx, ry, a0, a1))]


def path(*parts):
    """Joins points and point lists (from arc_pts) into one stroke, so its corners get round joins."""
    pts = []
    for p in parts:
        if isinstance(p, list):
            pts.extend(p)
        else:
            pts.append(p)
    return [LineString(pts)]


def ring(cx, cy, rx, ry):
    return arc(cx, cy, rx, ry, 0, 360)


def dot(x, y, r=H * 1.12):
    return [("dot", x, y, r)]


def stroke(parts, sw=SW):
    geoms = []
    for p in parts:
        if isinstance(p, tuple) and p[0] == "dot":
            geoms.append(Point(p[1], p[2]).buffer(p[3], quad_segs=12))
        elif isinstance(p, tuple) and p[0] == "fill":
            geoms.append(Polygon(p[1]).buffer(sw / 4, quad_segs=8))
        elif isinstance(p, tuple) and p[0] == "thin":
            geoms.append(stroke(p[2], p[1]))
        else:
            geoms.append(p.buffer(sw / 2, quad_segs=12))
    return unary_union(geoms)


def thin(sw, parts):
    """Strokes `parts` at width `sw` (crowded letters such as M, W, m, w)."""
    return [("thin", sw, parts)]


def fill(*pts):
    return [("fill", pts)]


# ---- the Latin design ----
O_RX = (R - L) / 2


def bowl_right(top, bottom, x0=L, x1=R):
    """A D-shaped bowl attached to a stem at x0, from `top` to `bottom` (centre lines)."""
    ry = (top - bottom) / 2
    rx = min(ry, (x1 - x0) * 0.55)
    cy = (top + bottom) / 2
    return path((x0, top), (x1 - rx, top), arc_pts(x1 - rx, cy, rx, ry, 90, -90), (x0, bottom))


def hump(x0, x1, top, low):
    """n's arch: up from the stem at x0 at height `low`, over `top`, down to x1 and the baseline."""
    rx = (x1 - x0) / 2
    return path(arc_pts((x0 + x1) / 2, low, rx, top - low, 180, 0), (x1, BASE))


G = {}


def glyph(ch, sw=SW):
    def deco(fn):
        G[ch] = (fn, sw)
        return fn
    return deco


def define_latin():
    g = G
    # uppercase
    g["A"] = (lambda: seg((L - 5, BASE), (MID, CAP + 5), (R + 5, BASE)) + seg((L + 60, BASE + 175), (R - 60, BASE + 175)), SW)
    g["B"] = (lambda: seg((L, BASE), (L, CAP)) + bowl_right(CAP, CMID + 15, L, R - 25) + bowl_right(CMID + 15, BASE, L, R), SW)
    g["C"] = (lambda: arc(MID + 15, CMID, O_RX + 15, (CAP - BASE) / 2, 40, 320), SW)
    g["D"] = (lambda: seg((L, BASE), (L, CAP)) + path((L, CAP), (R - 130, CAP), arc_pts(R - 130, CMID, 130, (CAP - BASE) / 2, 90, -90), (L, BASE)), SW)
    g["E"] = (lambda: path((R, CAP), (L, CAP), (L, BASE), (R, BASE)) + seg((L, CMID + 10), (R - 30, CMID + 10)), SW)
    g["F"] = (lambda: path((R, CAP), (L, CAP), (L, BASE)) + seg((L, CMID + 10), (R - 40, CMID + 10)), SW)
    g["G"] = (lambda: path(arc_pts(MID + 10, CMID, O_RX + 10, (CAP - BASE) / 2, 45, 360), (R + 5, CMID - 5), (MID + 30, CMID - 5)), SW)
    g["H"] = (lambda: seg((L, BASE), (L, CAP)) + seg((R, BASE), (R, CAP)) + seg((L, CMID + 10), (R, CMID + 10)), SW)
    g["I"] = (lambda: seg((MID, BASE), (MID, CAP)) + seg((MID - 105, CAP), (MID + 105, CAP)) + seg((MID - 105, BASE), (MID + 105, BASE)), SW)
    g["J"] = (lambda: path((R - 10, CAP), (R - 10, BASE + 140), arc_pts(MID - 5, BASE + 140, R - 10 - MID + 5, 140, 0, -180)), SW)
    g["K"] = (lambda: seg((L, BASE), (L, CAP)) + seg((R + 5, CAP), (L + 20, CMID - 20)) + seg((L + 140, CMID + 50), (R + 10, BASE)), SW)
    g["L"] = (lambda: path((L + 10, CAP), (L + 10, BASE), (R, BASE)), SW)
    g["M"] = (lambda: thin(96, path((L - 15, BASE), (L - 5, CAP), (MID, CMID - 60), (R + 5, CAP), (R + 15, BASE))), SW)
    g["N"] = (lambda: path((L, BASE), (L, CAP), (R, BASE), (R, CAP)), SW)
    g["O"] = (lambda: ring(MID, CMID, O_RX + 10, (CAP - BASE) / 2), SW)
    g["P"] = (lambda: seg((L, BASE), (L, CAP)) + bowl_right(CAP, CMID - 25, L, R), SW)
    g["Q"] = (lambda: ring(MID, CMID + 10, O_RX + 10, (CAP - BASE) / 2 - 10) + seg((MID + 50, BASE + 120), (R + 20, BASE - 25)), SW)
    g["R"] = (lambda: seg((L, BASE), (L, CAP)) + bowl_right(CAP, CMID - 15, L, R) + seg((MID - 10, CMID - 15), (R + 5, BASE)), SW)
    g["S"] = (lambda: s_curve(BASE, CAP, L - 5, R + 5), SW)
    g["T"] = (lambda: seg((L - 30, CAP), (R + 30, CAP)) + seg((MID, CAP), (MID, BASE)), SW)
    g["U"] = (lambda: path((L, CAP), (L, BASE + O_RX), arc_pts(MID, BASE + O_RX, O_RX, O_RX, 180, 360), (R, CAP)), SW)
    g["V"] = (lambda: path((L - 15, CAP), (MID, BASE), (R + 15, CAP)), SW)
    g["W"] = (lambda: thin(96, path((L - 30, CAP), (L + 45, BASE), (MID, CAP - 170), (R - 45, BASE), (R + 30, CAP))), SW)
    g["X"] = (lambda: seg((L - 5, CAP), (R + 5, BASE)) + seg((R + 5, CAP), (L - 5, BASE)), SW)
    g["Y"] = (lambda: path((L - 15, CAP), (MID, CMID + 20), (R + 15, CAP)) + seg((MID, CMID + 20), (MID, BASE)), SW)
    g["Z"] = (lambda: path((L, CAP), (R, CAP), (L, BASE), (R + 5, BASE)), SW)

    # lowercase
    xr = (XH - BASE) / 2
    g["a"] = (lambda: path(arc_pts(MID, XH - 120, O_RX - 10, 120, 160, 0), (R - 10, BASE - 10), (R + 15, BASE - 30))
              + path(arc_pts(MID - 15, BASE + 105, O_RX - 25, 105, 90, 450)), SW)
    g["b"] = (lambda: seg((L, ASC), (L, BASE)) + path(arc_pts(MID + 15, XMID, O_RX - 15, xr, 150, -150)), SW)
    g["c"] = (lambda: arc(MID + 10, XMID, O_RX + 10, xr, 45, 315), SW)
    g["d"] = (lambda: seg((R, ASC), (R, BASE)) + path(arc_pts(MID - 15, XMID, O_RX - 15, xr, 30, 330)), SW)
    g["e"] = (lambda: path((L + 5, XMID), (R + 5, XMID), arc_pts(MID + 5, XMID, O_RX, xr, 0, 315)), SW)
    g["f"] = (lambda: path((MID - 30, BASE), (MID - 30, ASC - 120), arc_pts(MID + 90, ASC - 120, 120, 120, 180, 60)) + seg((L - 15, XH), (R - 30, XH)), SW)
    g["g"] = (lambda: ring(MID - 10, XMID + 50, O_RX - 10, xr - 50) + path((R, XH), (R, DSC + 110), arc_pts(MID - 5, DSC + 110, R - MID + 5, 110, 0, -165)), SW)
    g["h"] = (lambda: seg((L, ASC), (L, BASE)) + hump(L, R, XH, XH - 150), SW)
    g["i"] = (lambda: seg((MID - 70, XH), (MID, XH), (MID, BASE)) + seg((MID - 90, BASE), (MID + 110, BASE)) + dot(MID, ASC + 5), SW)
    g["j"] = (lambda: path((MID - 40, XH), (MID + 40, XH), (MID + 40, DSC + 90), arc_pts(MID - 50, DSC + 90, 90, 90, 0, -150)) + dot(MID + 40, ASC + 5), SW)
    g["k"] = (lambda: seg((L, ASC), (L, BASE)) + seg((R, XH), (L + 20, BASE + 130)) + seg((L + 130, BASE + 190), (R + 10, BASE)), SW)
    g["l"] = (lambda: path((MID - 80, ASC), (MID - 5, ASC), (MID - 5, BASE + 70), arc_pts(MID + 80, BASE + 70, 85, 70, 180, 270), (R - 10, BASE)), SW)
    g["m"] = (lambda: thin(92, seg((L - 25, XH), (L - 25, BASE)) + hump(L - 25, MID, XH, XH - 130) + hump(MID, R + 25, XH, XH - 130)), SW)
    g["n"] = (lambda: seg((L, XH), (L, BASE)) + hump(L, R, XH, XH - 150), SW)
    g["o"] = (lambda: ring(MID, XMID, O_RX + 5, xr), SW)
    g["p"] = (lambda: seg((L, XH), (L, DSC)) + path(arc_pts(MID + 15, XMID, O_RX - 15, xr, 150, -150)), SW)
    g["q"] = (lambda: seg((R, XH), (R, DSC)) + path(arc_pts(MID - 15, XMID, O_RX - 15, xr, 30, 330)), SW)
    g["r"] = (lambda: seg((L + 20, XH), (L + 20, BASE)) + path(arc_pts(MID + 70, XH - 170, MID + 70 - L - 20, 170, 180, 70), (R + 10, XH - 20)), SW)
    g["s"] = (lambda: s_curve(BASE, XH, L, R), SW)
    g["t"] = (lambda: path((MID - 40, ASC - 20), (MID - 40, BASE + 90), arc_pts(MID + 50, BASE + 90, 90, 90, 180, 300)) + seg((L - 10, XH), (R - 10, XH)), SW)
    g["u"] = (lambda: path((L, XH), (L, BASE + 150), arc_pts(MID, BASE + 150, O_RX, 150, 180, 360)) + seg((R, XH), (R, BASE)), SW)
    g["v"] = (lambda: path((L - 15, XH), (MID, BASE), (R + 15, XH)), SW)
    g["w"] = (lambda: thin(92, path((L - 30, XH), (L + 45, BASE), (MID, XH - 140), (R - 45, BASE), (R + 30, XH))), SW)
    g["x"] = (lambda: seg((L, XH), (R, BASE)) + seg((R, XH), (L, BASE)), SW)
    g["y"] = (lambda: seg((L - 10, XH), (MID + 10, BASE + 20)) + path((R + 10, XH), (MID - 10, DSC + 40), (L - 10, DSC)), SW)
    g["z"] = (lambda: path((L, XH), (R, XH), (L, BASE), (R + 5, BASE)), SW)

    # digits (like the game's: round and heavy)
    g["0"] = (lambda: ring(MID, CMID, O_RX, (CAP - BASE) / 2), SW)
    g["1"] = (lambda: path((MID - 95, CAP - 70), (MID + 15, CAP), (MID + 15, BASE)), SW)
    g["2"] = (lambda: path(arc_pts(MID, CAP - 130, O_RX - 5, 130, 165, -20), (L, BASE), (R + 10, BASE)), SW)
    g["3"] = (lambda: arc(MID, CAP - 125, O_RX - 15, 125, 155, -90) + arc(MID, BASE + 140, O_RX, 140, 90, -160), SW)
    g["4"] = (lambda: path((R - 40, BASE), (R - 40, CAP), (L - 10, BASE + 165), (R + 25, BASE + 165)), SW)
    g["5"] = (lambda: path((R, CAP), (L + 15, CAP), (L, CMID + 25), arc_pts(MID, BASE + 150, O_RX, 150, 125, -155)), SW)
    g["6"] = (lambda: path(arc_pts(MID + 20, CMID + 70, O_RX + 20, (CAP - BASE) / 2 - 70 + 70, 60, 180), arc_pts(MID, BASE + 150, O_RX, 150, 180, 540)), SW)
    g["7"] = (lambda: path((L - 5, CAP), (R + 10, CAP), (MID - 20, BASE)), SW)
    g["8"] = (lambda: ring(MID, CAP - 120, O_RX - 25, 120) + ring(MID, BASE + 140, O_RX, 140), SW)
    g["9"] = (lambda: path(arc_pts(MID, CAP - 150, O_RX, 150, 0, 360), (R, CAP - 150), (R - 30, CMID - 60), arc_pts(MID - 20, CMID - 60, R - 30 - MID + 20, CMID - 60 - BASE, 0, -120)), SW)

    # punctuation and symbols
    g["!"] = (lambda: seg((MID, CAP + 10), (MID, BASE + 190)) + dot(MID, BASE + 10), SW)
    g["?"] = (lambda: path(arc_pts(MID, CAP - 120, O_RX - 20, 120, 165, -70), (MID, CMID - 30), (MID, BASE + 190)) + dot(MID, BASE + 10), SW)
    g["¡"] = (lambda: seg((MID, XH - 190), (MID, DSC - 10)) + dot(MID, XH - 10), SW)
    g["¿"] = (lambda: path(arc_pts(MID, DSC + 120, O_RX - 20, 120, 195, 430), (MID, XMID - 10), (MID, XH - 190)) + dot(MID, XH), SW)
    g["."] = (lambda: dot(MID, BASE + 10, H * 1.25), SW)
    g[","] = (lambda: dot(MID, BASE + 10, H * 1.25) + seg((MID + 30, BASE + 10), (MID - 40, BASE - 130)), SW)
    g[":"] = (lambda: dot(MID, BASE + 10, H * 1.2) + dot(MID, XH - 40, H * 1.2), SW)
    g[";"] = (lambda: dot(MID, BASE + 10, H * 1.2) + seg((MID + 30, BASE + 10), (MID - 40, BASE - 130)) + dot(MID, XH - 40, H * 1.2), SW)
    g["'"] = (lambda: seg((MID, CAP + 30), (MID, CAP - 140)), SW)
    g['"'] = (lambda: seg((MID - 85, CAP + 30), (MID - 85, CAP - 140)) + seg((MID + 85, CAP + 30), (MID + 85, CAP - 140)), SW)
    g["`"] = (lambda: seg((MID - 60, CAP + 50), (MID + 50, CAP - 50)), SW)
    g["-"] = (lambda: seg((L + 10, CMID - 30), (R - 10, CMID - 30)), SW)
    g["–"] = (lambda: seg((L - 40, CMID - 30), (R + 40, CMID - 30)), SW)
    g["—"] = (lambda: seg((-20, CMID - 30), (HALF + 20, CMID - 30)), SW)
    g["_"] = (lambda: seg((L - 40, DSC + 20), (R + 40, DSC + 20)), SW)
    g["+"] = (lambda: seg((L - 10, CMID - 20), (R + 10, CMID - 20)) + seg((MID, CMID - 175), (MID, CMID + 135)), SW)
    g["="] = (lambda: seg((L - 10, CMID + 60), (R + 10, CMID + 60)) + seg((L - 10, CMID - 100), (R + 10, CMID - 100)), SW)
    g["*"] = (lambda: sum((seg((MID, CMID + 20), (MID + 165 * math.sin(math.radians(a)), CMID + 20 + 165 * math.cos(math.radians(a)))) for a in range(0, 360, 60)), []), SW)
    g["/"] = (lambda: seg((R + 10, CAP + 20), (L - 10, BASE - 40)), SW)
    g["\\"] = (lambda: seg((L - 10, CAP + 20), (R + 10, BASE - 40)), SW)
    g["|"] = (lambda: seg((MID, ASC + 30), (MID, DSC)), SW)
    g["¦"] = (lambda: seg((MID, ASC + 30), (MID, CMID + 70)) + seg((MID, CMID - 80), (MID, DSC)), SW)
    g["("] = (lambda: arc(R + 60, CMID, 220, (CAP - BASE) / 2 + 90, 130, 230), SW)
    g[")"] = (lambda: arc(L - 60, CMID, 220, (CAP - BASE) / 2 + 90, 50, -50), SW)
    g["["] = (lambda: path((R - 40, CAP + 60), (MID - 60, CAP + 60), (MID - 60, BASE - 80), (R - 40, BASE - 80)), SW)
    g["]"] = (lambda: path((L + 40, CAP + 60), (MID + 60, CAP + 60), (MID + 60, BASE - 80), (L + 40, BASE - 80)), SW)
    g["{"] = (lambda: path((R - 20, CAP + 60), (MID - 10, CAP + 40), (MID - 30, CMID + 30), (L, CMID - 20), (MID - 30, CMID - 70), (MID - 10, BASE - 60), (R - 20, BASE - 80)), SW)
    g["}"] = (lambda: path((L + 20, CAP + 60), (MID + 10, CAP + 40), (MID + 30, CMID + 30), (R, CMID - 20), (MID + 30, CMID - 70), (MID + 10, BASE - 60), (L + 20, BASE - 80)), SW)
    g["<"] = (lambda: path((R, CMID + 140), (L, CMID - 20), (R, CMID - 180)), SW)
    g[">"] = (lambda: path((L, CMID + 140), (R, CMID - 20), (L, CMID - 180)), SW)
    g["«"] = (lambda: thin(90, path((MID - 10, XH - 40), (L - 20, XMID), (MID - 10, BASE + 40)) + path((R + 20, XH - 40), (MID + 30, XMID), (R + 20, BASE + 40))), SW)
    g["»"] = (lambda: thin(90, path((L - 20, XH - 40), (MID - 10, XMID), (L - 20, BASE + 40)) + path((MID + 30, XH - 40), (R + 20, XMID), (MID + 30, BASE + 40))), SW)
    g["^"] = (lambda: path((L, CMID + 60), (MID, CAP + 10), (R, CMID + 60)), SW)
    g["~"] = (lambda: wave(CMID - 20, 60), SW)
    g["#"] = (lambda: thin(90, seg((MID - 75, CAP + 10), (MID - 105, BASE - 10)) + seg((MID + 105, CAP + 10), (MID + 75, BASE - 10)) + seg((L - 30, CMID + 85), (R + 30, CMID + 85)) + seg((L - 30, CMID - 95), (R + 30, CMID - 95))), SW)
    g["%"] = (lambda: thin(84, ring(L + 30, CAP - 75, 70, 85) + ring(R - 30, BASE + 75, 70, 85) + seg((R + 20, CAP + 10), (L - 20, BASE - 10))), SW)
    g["&"] = (lambda: path((R + 20, BASE - 10), (L + 30, CMID + 60), arc_pts(MID - 10, CAP - 85, 95, 95, 210, -30), (L, CMID - 80), arc_pts(MID - 30, BASE + 120, MID - 30 - L, 120, 180, 330), (R + 10, CMID + 10)), SW)
    g["@"] = (lambda: thin(84, ring(MID + 20, CMID - 10, 85, 100) + path((MID + 105, CMID + 90), (MID + 105, CMID - 70), arc_pts(MID + 155, CMID - 70, 50, 60, 180, 360), arc_pts(MID, CMID, R + 40 - MID, (CAP - BASE) / 2 + 20, 10, 330))), SW)
    g["$"] = (lambda: s_curve(BASE + 20, CAP - 20, L - 5, R + 5) + seg((MID, CAP + 80), (MID, BASE - 80)), SW)
    g["¢"] = (lambda: arc(MID + 10, XMID, O_RX, xr, 45, 315) + seg((MID + 20, XH + 70), (MID + 20, BASE - 70)), SW)
    g["£"] = (lambda: path(arc_pts(MID + 60, CAP - 110, 120, 110, 30, 180), (L + 70, BASE + 40), (L - 10, BASE), (R + 10, BASE)) + seg((L - 10, CMID - 20), (R - 60, CMID - 20)), SW)
    g["¥"] = (lambda: path((L - 15, CAP), (MID, CMID + 40), (R + 15, CAP)) + seg((MID, CMID + 40), (MID, BASE)) + seg((L, CMID - 50), (R, CMID - 50)), SW)
    g["¤"] = (lambda: ring(MID, CMID - 10, 110, 110) + seg((L - 10, CMID + 150), (L + 50, CMID + 90)) + seg((R + 10, CMID + 150), (R - 50, CMID + 90)) + seg((L - 10, CMID - 170), (L + 50, CMID - 110)) + seg((R + 10, CMID - 170), (R - 50, CMID - 110)), SW)
    g["§"] = (lambda: thin(92, s_curve(CMID - 30, CAP + 20, L + 10, R - 10) + s_curve(BASE - 30, CMID + 30, L + 10, R - 10)), SW)
    g["¶"] = (lambda: fill((MID, CAP), (MID - 70, CAP), (L - 20, CAP - 90), (L - 20, CMID + 40), (MID - 70, CMID - 30), (MID, CMID - 30)) + seg((MID, CAP), (MID, BASE - 40)) + seg((R, CAP), (R, BASE - 40)) + seg((MID, CAP), (R, CAP)), SW)
    g["·"] = (lambda: dot(MID, CMID - 20, H * 1.2), SW)
    g["•"] = (lambda: dot(MID, CMID - 20, 110), SW)
    g["…"] = (lambda: dot(MID - 155, BASE + 10, H * 0.9) + dot(MID, BASE + 10, H * 0.9) + dot(MID + 155, BASE + 10, H * 0.9), SW)
    g["‘"] = (lambda: dot(MID, CAP - 30, H * 1.15) + seg((MID - 25, CAP - 30), (MID + 40, CAP + 100)), SW)
    g["’"] = (lambda: dot(MID, CAP + 30, H * 1.15) + seg((MID + 25, CAP + 30), (MID - 40, CAP - 100)), SW)
    g["“"] = (lambda: dot(MID - 85, CAP - 30, H * 1.05) + seg((MID - 105, CAP - 30), (MID - 45, CAP + 100)) + dot(MID + 85, CAP - 30, H * 1.05) + seg((MID + 65, CAP - 30), (MID + 125, CAP + 100)), SW)
    g["”"] = (lambda: dot(MID - 85, CAP + 30, H * 1.05) + seg((MID - 65, CAP + 30), (MID - 125, CAP - 100)) + dot(MID + 85, CAP + 30, H * 1.05) + seg((MID + 105, CAP + 30), (MID + 45, CAP - 100)), SW)
    g["„"] = (lambda: dot(MID - 85, BASE + 10, H * 1.05) + seg((MID - 65, BASE + 10), (MID - 125, BASE - 120)) + dot(MID + 85, BASE + 10, H * 1.05) + seg((MID + 105, BASE + 10), (MID + 45, BASE - 120)), SW)
    g["‚"] = (lambda: G[","][0](), SW)
    g["°"] = (lambda: thin(84, ring(MID, CAP - 70, 85, 85)), SW)
    g["ª"] = (lambda: thin(84, ring(MID - 15, CAP - 95, 85, 75) + seg((MID + 75, CAP), (MID + 75, CAP - 170)) + seg((L, CMID - 25), (R, CMID - 25))), SW)
    g["º"] = (lambda: thin(84, ring(MID, CAP - 90, 95, 80) + seg((L, CMID - 25), (R, CMID - 25))), SW)
    g["¹"] = (lambda: thin(80, path((MID - 70, CAP - 50), (MID + 10, CAP + 20), (MID + 10, CMID - 30))), SW)
    g["²"] = (lambda: thin(80, path(arc_pts(MID, CAP - 50, 85, 70, 165, -20), (L + 30, CMID - 30), (R - 20, CMID - 30))), SW)
    g["³"] = (lambda: thin(80, arc(MID, CAP - 40, 80, 60, 155, -90) + arc(MID, CMID + 40, 90, 70, 90, -160)), SW)
    g["±"] = (lambda: seg((L - 10, CMID + 40), (R + 10, CMID + 40)) + seg((MID, CMID + 190), (MID, CMID - 110)) + seg((L - 10, BASE), (R + 10, BASE)), SW)
    g["×"] = (lambda: seg((L + 10, CMID + 110), (R - 10, CMID - 150)) + seg((R - 10, CMID + 110), (L + 10, CMID - 150)), SW)
    g["÷"] = (lambda: seg((L - 10, CMID - 20), (R + 10, CMID - 20)) + dot(MID, CMID + 140) + dot(MID, CMID - 180), SW)
    g["¬"] = (lambda: path((L - 10, CMID + 40), (R, CMID + 40), (R, CMID - 110)), SW)
    g["¯"] = (lambda: seg((L, CAP + 100), (R, CAP + 100)), SW)
    g["´"] = (lambda: accent("acute", CAP - 100), SW)
    g["¨"] = (lambda: accent("diaeresis", CAP - 100), SW)
    g["¸"] = (lambda: accent("cedilla", 0), SW)
    g["µ"] = (lambda: seg((L, XH), (L, DSC)) + path((L, BASE + 150), arc_pts(MID, BASE + 150, O_RX, 150, 180, 360)) + seg((R, XH), (R, BASE)), SW)
    g["©"] = (lambda: thin(76, ring(MID, CMID, 205, 245) + arc(MID + 10, CMID, 90, 110, 45, 315)), SW)
    g["®"] = (lambda: thin(76, ring(MID, CMID, 205, 245) + seg((MID - 70, CMID - 130), (MID - 70, CMID + 120)) + bowl_right(CMID + 120, CMID, MID - 70, MID + 90) + seg((MID, CMID), (MID + 90, CMID - 130))), SW)
    g["¼"] = (lambda: fraction("1", "4"), SW)
    g["½"] = (lambda: fraction("1", "2"), SW)
    g["¾"] = (lambda: fraction("3", "4"), SW)
    g["ß"] = (lambda: path((L, BASE), (L, ASC - 120), arc_pts(MID, ASC - 120, MID - L, 120, 180, -30), (MID - 20, CMID + 10), arc_pts(MID + 10, BASE + 130, R - MID - 10, 130 + 20, 90, -125)), SW)
    g["Æ"] = (lambda: thin(92, path((L - 20, BASE), (MID - 10, CAP), (R + 10, CAP)) + path((MID, CAP), (MID, BASE), (R + 10, BASE)) + seg((L + 40, BASE + 175), (MID, BASE + 175)) + seg((MID, CMID + 10), (R - 10, CMID + 10))), SW)
    g["æ"] = (lambda: thin(88, path(arc_pts(MID - 70, XH - 100, 100, 100, 160, 0), (MID + 30, BASE + 30)) + ring(MID - 85, BASE + 95, 95, 95) + path((MID + 30, XMID), (R + 25, XMID), arc_pts(MID + 125, XMID, R + 25 - MID - 125, xr, 0, 300))), SW)
    g["Ø"] = (lambda: G["O"][0]() + seg((R + 25, CAP + 30), (L - 25, BASE - 30)), SW)
    g["ø"] = (lambda: G["o"][0]() + seg((R + 25, XH + 30), (L - 25, BASE - 30)), SW)
    g["Ð"] = (lambda: G["D"][0]() + seg((L - 40, CMID), (MID, CMID)), SW)
    g["ð"] = (lambda: ring(MID, BASE + 160, O_RX, 160) + path((L + 60, ASC + 10), arc_pts(MID - 80, CMID - 30, R - MID + 80, ASC - CMID + 40, 60, -10)) + seg((MID - 70, ASC - 120), (R, ASC - 30)), SW)
    g["Þ"] = (lambda: seg((L, BASE), (L, CAP)) + bowl_right(CAP - 110, BASE + 110, L, R), SW)
    g["þ"] = (lambda: seg((L, ASC), (L, DSC)) + path(arc_pts(MID + 15, XMID, O_RX - 15, xr, 150, -150)), SW)
    g["→"] = (lambda: seg((L - 50, CMID - 20), (R + 30, CMID - 20)) + path((MID + 10, CMID + 150), (R + 40, CMID - 20), (MID + 10, CMID - 190)), SW)
    g["←"] = (lambda: seg((R + 50, CMID - 20), (L - 30, CMID - 20)) + path((MID - 10, CMID + 150), (L - 40, CMID - 20), (MID - 10, CMID - 190)), SW)
    g["↑"] = (lambda: seg((MID, BASE - 20), (MID, CAP + 20)) + path((L - 10, CAP - 140), (MID, CAP + 30), (R + 10, CAP - 140)), SW)
    g["↓"] = (lambda: seg((MID, CAP + 20), (MID, BASE - 20)) + path((L - 10, BASE + 140), (MID, BASE - 30), (R + 10, BASE + 140)), SW)
    g["◀"] = (lambda: fill((R, CAP), (R, BASE), (L - 10, CMID)), SW)
    g["▶"] = (lambda: fill((L, CAP), (L, BASE), (R + 10, CMID)), SW)
    g["▲"] = (lambda: fill((L - 20, BASE + 20), (R + 20, BASE + 20), (MID, CAP)), SW)
    g["▼"] = (lambda: fill((L - 20, CAP), (R + 20, CAP), (MID, BASE + 20)), SW)
    g["♂"] = (lambda: thin(92, ring(MID - 45, BASE + 150, 125, 140) + seg((MID + 45, BASE + 260), (R + 20, CAP + 10)) + path((R - 130, CAP + 10), (R + 20, CAP + 10), (R + 20, CAP - 140))), SW)
    g["♀"] = (lambda: thin(92, ring(MID, CAP - 125, 135, 135) + seg((MID, CAP - 260), (MID, BASE - 70)) + seg((L - 10, BASE + 80), (R + 10, BASE + 80))), SW)
    # Latin Extended-A letters that are not base + accent
    g["ı"] = (lambda: seg((MID - 70, XH), (MID, XH), (MID, BASE)) + seg((MID - 90, BASE), (MID + 110, BASE)), SW)
    g["ȷ"] = (lambda: path((MID - 40, XH), (MID + 40, XH), (MID + 40, DSC + 90), arc_pts(MID - 50, DSC + 90, 90, 90, 0, -150)), SW)
    g["Œ"] = (lambda: thin(90, path((R + 15, CAP), (MID + 20, CAP), arc_pts(MID + 20, CMID, MID + 20 - L + 25, (CAP - BASE) / 2, 90, 270), (R + 15, BASE)) + seg((MID + 20, CAP), (MID + 20, BASE)) + seg((MID + 20, CMID + 10), (R, CMID + 10))), SW)
    g["œ"] = (lambda: thin(88, ring(MID - 95, XMID, 100, xr) + path((MID + 5, XMID), (R + 25, XMID), arc_pts(MID + 120, XMID, R + 25 - MID - 120, xr, 0, 300))), SW)
    g["Ł"] = (lambda: G["L"][0]() + seg((L - 40, CMID - 40), (L + 90, CMID + 60)), SW)
    g["ł"] = (lambda: G["l"][0]() + seg((MID - 110, CMID - 60), (MID + 90, CMID + 60)), SW)
    g["Đ"] = (lambda: G["Ð"][0](), SW)
    g["đ"] = (lambda: G["d"][0]() + seg((MID, ASC - 60), (R + 50, ASC - 60)), SW)
    g["Ħ"] = (lambda: G["H"][0]() + seg((L - 40, CAP - 110), (R + 40, CAP - 110)), SW)
    g["ħ"] = (lambda: G["h"][0]() + seg((L - 50, ASC - 80), (MID + 20, ASC - 80)), SW)
    g["Ŋ"] = (lambda: seg((L, BASE), (L, CAP)) + path(arc_pts(MID, CAP - 150, O_RX, 150, 180, 0), (R, BASE + 40), arc_pts(R - 100, BASE + 40, 100, 100, 0, -120)), SW)
    g["ŋ"] = (lambda: seg((L, XH), (L, BASE)) + path(arc_pts(MID, XH - 150, O_RX, 150, 180, 0), (R, DSC + 70), arc_pts(R - 100, DSC + 70, 100, 100, 0, -140)), SW)
    g["ĸ"] = (lambda: seg((L, XH), (L, BASE)) + seg((R, XH), (L + 20, BASE + 130)) + seg((L + 130, BASE + 190), (R + 10, BASE)), SW)
    g["Ĳ"] = (lambda: thin(90, seg((L - 10, CAP), (L - 10, BASE)) + path((R + 10, CAP), (R + 10, BASE + 110), arc_pts(MID + 60, BASE + 110, R + 10 - MID - 60, 110, 0, -170))), SW)
    g["ĳ"] = (lambda: thin(90, seg((L, XH), (L, BASE)) + dot(L, ASC + 5, H) + path((R, XH), (R, DSC + 80), arc_pts(MID + 70, DSC + 80, R - MID - 70, 80, 0, -170)) + dot(R, ASC + 5, H)), SW)
    g["Ŀ"] = (lambda: G["L"][0]() + dot(MID + 60, CMID + 10), SW)
    g["ŀ"] = (lambda: thin(96, path((L - 20, ASC), (L + 50, ASC), (L + 50, BASE))) + dot(R - 20, XMID + 30), SW)
    g["ŉ"] = (lambda: thin(92, dot(L - 10, CAP + 20, H) + seg((L + 10, CAP + 20), (L - 40, CAP - 90)) + seg((MID - 30, XH), (MID - 30, BASE)) + hump(MID - 30, R + 20, XH, XH - 140)), SW)
    g["ſ"] = (lambda: path((MID - 30, BASE), (MID - 30, ASC - 120), arc_pts(MID + 90, ASC - 120, 120, 120, 180, 60)), SW)
    g["€"] = (lambda: arc(MID + 40, CMID, O_RX, (CAP - BASE) / 2, 50, 310) + seg((L - 50, CMID + 60), (MID + 60, CMID + 60)) + seg((L - 50, CMID - 70), (MID + 40, CMID - 70)), SW)


def s_curve(bottom, top, x0, x1):
    """An S between centre lines `bottom` and `top`: two stacked bowls meeting in a slanted spine."""
    h = top - bottom
    ru = h * 0.255
    rl = h * 0.265
    rx = (x1 - x0) / 2
    cx = (x0 + x1) / 2
    yu = top - ru
    yl = bottom + rl
    return path(arc_pts(cx, yu, rx - 5, ru, 25, 270), arc_pts(cx, yl, rx, rl, 90, -155))


def wave(y, amp):
    return path([(L - 40 + (R - L + 80) * i / 24, y + amp * math.sin(i / 24 * 2 * math.pi)) for i in range(25)])


def fraction(a, b):
    # small numerator up left, denominator down right, slash between (thin strokes)
    def small(ch, dx, dy):
        parts = G[ch][0]()
        return [shift_scale(p, 0.42, dx, dy) for p in parts]
    return thin(72, small(a, -40, CAP * 0.52) + small(b, 230, -10) + seg((R + 20, CAP + 10), (L - 20, BASE - 10)))


def shift_scale(p, s, dx, dy):
    if isinstance(p, tuple):
        if p[0] == "dot":
            return ("dot", p[1] * s + dx, p[2] * s + dy, p[3] * s * 1.6)
        if p[0] == "thin":
            return ("thin", p[1], [shift_scale(q, s, dx, dy) for q in p[2]])
        return ("fill", [(x * s + dx, y * s + dy) for x, y in p[1]])
    return LineString([(x * s + dx, y * s + dy) for x, y in p.coords])


# ---- accents (diacritics). `top` is the height of the letter they sit on ----
def accent(kind, top):
    y = top + 105
    if kind == "acute":
        return seg((MID - 45, y - 15), (MID + 60, y + 85))
    if kind == "grave":
        return seg((MID + 45, y - 15), (MID - 60, y + 85))
    if kind == "circumflex":
        return thin(92, path((MID - 105, y - 20), (MID, y + 80), (MID + 105, y - 20)))
    if kind == "caron":
        return thin(92, path((MID - 105, y + 80), (MID, y - 20), (MID + 105, y + 80)))
    if kind == "tilde":
        return thin(88, path([(MID - 130 + 260 * i / 16, y + 30 + 45 * math.sin(i / 16 * 2 * math.pi)) for i in range(17)]))
    if kind == "diaeresis":
        return dot(MID - 95, y + 25, H * 1.15) + dot(MID + 95, y + 25, H * 1.15)
    if kind == "ring":
        return thin(80, ring(MID, y + 35, 62, 52))
    if kind == "macron":
        return seg((L + 10, y + 20), (R - 10, y + 20))
    if kind == "cedilla":
        return thin(84, path((MID + 10, BASE - 30), (MID + 10, -70), arc_pts(MID - 20, -120, 75, 55, 90, -150)))
    if kind == "breve":
        return thin(88, arc(MID, y + 75, 95, 85, 200, 340))
    if kind == "dot":
        return dot(MID, y + 25, H * 1.2)
    if kind == "doubleacute":
        return thin(84, seg((MID - 105, y - 15), (MID - 30, y + 85)) + seg((MID + 30, y - 15), (MID + 105, y + 85)))
    if kind == "ogonek":
        return thin(84, path((R - 20, BASE - 20), arc_pts(R - 20, -70, 70, 70, 150, 290), (R + 50, -120)))
    if kind == "commabelow":
        return dot(MID, -80, H * 1.05) + seg((MID + 25, -80), (MID - 40, -190))
    raise ValueError(kind)


MARKS = {"̀": "grave", "́": "acute", "̂": "circumflex", "̃": "tilde", "̈": "diaeresis",
         "̊": "ring", "̧": "cedilla", "̄": "macron", "̌": "caron",
         "\u0306": "breve", "\u0307": "dot", "\u030B": "doubleacute", "\u0328": "ogonek", "\u0326": "commabelow"}


def latin_parts(ch):
    """Skeleton parts of `ch`: a drawn glyph, or a base letter plus accents for precomposed letters (é = e + ´)."""
    import unicodedata
    if ch in G:
        return G[ch][0]()
    if ch == " " or ch == " ":
        return []
    d = unicodedata.normalize("NFD", ch)
    if len(d) >= 2 and d[0] in G and all(m in MARKS for m in d[1:]):
        base = d[0]
        if base == "i":
            parts = seg((MID - 70, XH), (MID, XH), (MID, BASE)) + seg((MID - 90, BASE), (MID + 110, BASE))  # dotless
        elif base == "j":
            parts = [p for p in G["j"][0]() if not (isinstance(p, tuple) and p[0] == "dot")]
        else:
            parts = G[base][0]()
        top = CAP + H if base.isupper() else XH + H
        if base in "bdfhklt":
            top = ASC + H
        for m in d[1:]:
            k = MARKS[m]
            if k in ("cedilla", "ogonek", "commabelow"):
                parts = parts + accent(k, 0)
            elif base.isupper():
                parts = parts + cap_accent(k)
            else:
                parts = parts + accent(k, top - 30)
        return parts
    return None


def cap_accent(kind):
    """Accents over capitals sit low and flat: the cell has 240 units over the caps, outline included."""
    parts = accent(kind, CAP - 60)
    return [squash(p, CAP + 70) for p in parts]


def squash(p, y0, s=0.72):
    if isinstance(p, tuple):
        if p[0] == "dot":
            return ("dot", p[1], y0 + (p[2] - y0) * s, p[3] * 0.92)
        if p[0] == "thin":
            return ("thin", p[1] * 0.9, [squash(q, y0, s) for q in p[2]])
        return p
    return LineString([(x, y0 + (y - y0) * s) for x, y in p.coords])


# ---- output ----
def polys_of(geom):
    if geom.is_empty:
        return []
    if isinstance(geom, Polygon):
        return [geom]
    if isinstance(geom, MultiPolygon):
        return list(geom.geoms)
    return [g for g in getattr(geom, "geoms", []) if isinstance(g, Polygon)]


def draw_shape(geom, pen):
    geom = geom.simplify(1.5)
    for poly in polys_of(geom):
        rings = [(poly.exterior, True)] + [(i, False) for i in poly.interiors]
        for ring_, outer in rings:
            pts = list(ring_.coords)[:-1]
            # TrueType: outer contours clockwise, holes counter-clockwise
            area = sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))
            if (area > 0) == outer:
                pts.reverse()
            pen.moveTo((round(pts[0][0]), round(pts[0][1])))
            for x, y in pts[1:]:
                pen.lineTo((round(x), round(y)))
            pen.closePath()


def latin_glyph(ch):
    parts = latin_parts(ch)
    if parts is None:
        return None
    pen = TTGlyphPen(None)
    if parts:
        sw = G[ch][1] if ch in G else SW
        draw_shape(stroke(parts, sw), pen)
    return pen.glyph()


def is_fullwidth(cp):
    return (0x2E80 <= cp < 0xA000) or (0xAC00 <= cp < 0xD7B0) or (0xFF00 <= cp < 0xFF61) or (0x3000 <= cp < 0x3100) or (0x1100 <= cp < 0x1200) or (0x3130 <= cp < 0x3190)


class FlattenPen(BasePen):
    """Collects a glyph's contours as polygons (curves flattened), for the emboldening below."""

    def __init__(self, glyphset=None):
        super().__init__(glyphset)
        self.contours, self.cur = [], []

    def _moveTo(self, pt):
        self.cur = [pt]

    def _lineTo(self, pt):
        self.cur.append(pt)

    def _qCurveToOne(self, pt1, pt2):
        p0 = self.cur[-1]
        for i in range(1, 7):
            t = i / 6
            self.cur.append(((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * pt1[0] + t * t * pt2[0],
                             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * pt1[1] + t * t * pt2[1]))

    def _curveToOne(self, pt1, pt2, pt3):
        p0 = self.cur[-1]
        for i in range(1, 9):
            t = i / 8
            a, b, c, d = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
            self.cur.append((a * p0[0] + b * pt1[0] + c * pt2[0] + d * pt3[0], a * p0[1] + b * pt1[1] + c * pt2[1] + d * pt3[1]))

    def _closePath(self):
        if len(self.cur) >= 3:
            self.contours.append(self.cur)
        self.cur = []

    _endPath = _closePath

    def shape(self):
        geom = Polygon()
        for c in self.contours:  # even-odd fill
            geom = geom.symmetric_difference(Polygon(c).buffer(0))
        return geom


class Base:
    def __init__(self, path, scale, src_cy, xscale=1.0, embolden=0, round_corners=0):
        self.font = TTFont(path)
        self.cmap = self.font.getBestCmap()
        self.gs = self.font.getGlyphSet()
        self.scale = scale
        self.xscale = xscale
        self.src_cy = src_cy
        self.embolden = embolden  # units (of this font) to grow every outline by, for a heavier weight
        self.round_corners = round_corners  # units: erode then grow back by this much, which rounds square corners

    def glyph(self, cp, adv=FULL):
        name = self.cmap.get(cp)
        if name is None:
            return None
        rec = DecomposingRecordingPen(self.gs)
        self.gs[name].draw(rec)
        from fontTools.pens.boundsPen import BoundsPen
        bp = BoundsPen(self.gs)
        self.gs[name].draw(bp)
        pen = TTGlyphPen(None)
        s = self.scale
        # centre the ink horizontally in the full-width cell; keep the source's vertical position, rescaled about its
        # em centre so it lands on this font's cell centre
        src_cx = self.gs[name].width / 2 if bp.bounds is None else (bp.bounds[0] + bp.bounds[2]) / 2
        if cp in (0x3001, 0x3002, 0xFF0C, 0xFF0E) or 0x300C <= cp <= 0x300D:
            src_cx = self.gs[name].width / 2  # keep the punctuation's own place in its cell
        dst_cy = (ASCENT - DESCENT) / 2
        sx = s * self.xscale
        matrix = (sx, 0, 0, s, adv / 2 - src_cx * sx, dst_cy - self.src_cy * s)
        if self.embolden or self.round_corners:
            flat = FlattenPen()
            rec.replay(TransformPen(flat, matrix))
            shape = flat.shape()
            if self.round_corners:
                shape = shape.buffer(-self.round_corners * s, quad_segs=6)
            shape = shape.buffer((self.embolden + self.round_corners) * s, quad_segs=6)
            draw_shape(shape, pen)
        else:
            rec.replay(TransformPen(pen, matrix))
        return pen.glyph()


def build(args):
    define_latin()
    chars = wanted_chars() + list("–—‘’“”„‚…•←↑↓▲▼▶€")
    mplus = Base(args.mplus, 0.9, 360)  # em box -140..860
    zen = nanum = None
    if args.full:
        # every block the Stadium games' languages could use: Latin-1 and Latin Extended-A (drawn here), the
        # general punctuation drawn here, all of M+'s kana, kanji, CJK symbols and fullwidth/halfwidth forms, the
        # kanji M+ lacks from Zen Maru Gothic (JIS levels 1 and 2 and more), and all 11,172 Hangul syllables
        zen = Base(args.zen, 0.9, 380)  # em box ~ -90..850
        # syllables Jua lacks (it has the 2,350 of KS X 1001): Nanum Gothic ExtraBold, corners rounded
        nanum = Base(args.nanum, 0.8, 370, xscale=1.08, round_corners=28)
        extra = set(chr(c) for c in range(0x100, 0x180))
        extra |= set(chr(c) for c in mplus.cmap if 0x3000 <= c < 0x3100 or 0x31F0 <= c < 0x3200 or 0x3400 <= c < 0xA000
                     or 0xF900 <= c < 0xFB00 or 0xFF00 <= c < 0xFFF0 or 0x2460 <= c < 0x2800)
        extra |= set(chr(c) for c in zen.cmap if 0x3400 <= c < 0xA000 or 0xF900 <= c < 0xFB00)
        extra |= set(chr(c) for c in range(0xAC00, 0xD7A4)) | set(chr(c) for c in range(0x3131, 0x318F))
        chars = chars + sorted(extra)
    chars = sorted(set(chars) | {" ", "?"})
    # Hangul ink -30..810; Jua's syllables are narrow and light next to the rest: widened to fill the monospaced
    # cell and grown a little to match M+ ExtraBold's weight
    jua = Base(args.jua, 0.9, 390, xscale=1.12, embolden=22)
    glyphs = {".notdef": TTGlyphPen(None).glyph()}
    metrics = {".notdef": (HALF, 0)}
    cmap = {}
    missing = []
    for ch in chars:
        cp = ord(ch)
        name = f"uni{cp:04X}"
        g = None
        adv = HALF
        if is_fullwidth(cp):
            adv = FULL
            if 0xAC00 <= cp < 0xD7B0 or 0x3130 <= cp < 0x3190 or 0x1100 <= cp < 0x1200:
                g = jua.glyph(cp)
                if g is None and nanum is not None:
                    g = nanum.glyph(cp)
            if g is None:
                g = mplus.glyph(cp)
            if g is None and zen is not None:
                g = zen.glyph(cp)
            if cp == 0x3000:
                g = TTGlyphPen(None).glyph()
        else:
            g = latin_glyph(ch)
            if g is None and 0xFF61 <= cp < 0xFFA0:  # halfwidth katakana stay half width
                g = mplus.glyph(cp, HALF)
            elif g is None:  # anything else the design lacks (symbols such as circled digits, ♪): M+, full width
                g = mplus.glyph(cp)
                if g is not None:
                    adv = FULL
        if g is None:
            missing.append(ch)
            continue
        glyphs[name] = g
        cmap[cp] = name
        g.recalcBounds(glyphs)
        metrics[name] = (adv, getattr(g, "xMin", 0))
    fb = FontBuilder(UPM, isTTF=True)
    order = list(glyphs)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=ASCENT, descent=-DESCENT)
    fb.setupNameTable({
        "familyName": "Plethron Pop",
        "styleName": "Regular",
        "copyright": "Latin: Copyright 2026 Team Hippodrome. Kana and kanji from M PLUS Rounded 1c, Copyright 2016 The "
                     "Rounded M+ Project Authors. Hangul from Jua, Copyright 2018 The Jua Project Authors."
                     + (" Further kanji from Zen Maru Gothic, Copyright 2021 The Zen Maru Gothic Project Authors. "
                        "Further Hangul from Nanum Gothic, Copyright 2010 NHN Corporation." if args.full else ""),
        "licenseDescription": "This Font Software is licensed under the SIL Open Font License, Version 1.1.",
        "licenseInfoURL": "https://openfontlicense.org",
    })
    fb.setupOS2(sTypoAscender=ASCENT, sTypoDescender=-DESCENT, usWinAscent=ASCENT, usWinDescent=DESCENT)
    fb.setupPost(isFixedPitch=0)
    fb.save(args.out)
    print(f"{args.out}: {len(cmap)} characters, {os.path.getsize(args.out) // 1024} KB")
    if missing:
        print("not covered (Unifont fallback):", "".join(missing))
    return chars


def preview(path, out, text=None):
    from PIL import Image, ImageDraw, ImageFont
    font = ImageFont.truetype(path, 96)
    lines = text or [
        "ABCDEFGHIJKLM", "NOPQRSTUVWXYZ", "abcdefghijklm", "nopqrstuvwxyz", "0123456789!?.,:;",
        "ÁÉÍÓÚÑÜÇ ÃÕÂÊÔ ßäöü", "¿¡«»“„”’…→♀♂%&+-=/()", "POKéMON STADIUM", "Pikachu used Thunder!",
    ]
    img = Image.new("RGB", (96 * 14, 104 * len(lines) + 20), (60, 90, 160))
    d = ImageDraw.Draw(img)
    for i, line in enumerate(lines):
        d.text((20, 10 + i * 104), line, font=font, fill=(255, 255, 255))
    img.save(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mplus", required=True, help="MPLUSRounded1c-ExtraBold.ttf")
    ap.add_argument("--jua", required=True, help="Jua-Regular.ttf")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--full", action="store_true",
                    help="every kana, kanji (JIS 1+2 and more) and Hangul syllable, plus Latin Extended-A, not only "
                         "what the tables use (for editing and other games; needs --zen and --nanum)")
    ap.add_argument("--zen", help="ZenMaruGothic-Black.ttf (with --full)")
    ap.add_argument("--nanum", help="NanumGothic-ExtraBold.ttf (with --full)")
    ap.add_argument("--preview", help="also write a specimen PNG of the result")
    args = ap.parse_args()
    build(args)
    if args.preview:
        preview(args.out, args.preview)


if __name__ == "__main__":
    main()
