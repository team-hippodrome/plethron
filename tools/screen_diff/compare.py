#!/usr/bin/env python3 -I
"""Scores the port's pictures against ares's and writes a ranked report.

Usage: tools/screen_diff/compare.py <report dir> <run dir> [<run dir> ...]   (run dirs from tools/screen_diff/run.py)

Needs numpy and Pillow (pip install numpy pillow).

For each checkpoint both pictures are brought to the N64's 320x240 (box filter: ares's 640x480 VI output, the port's
native frame), then the port picture is compared with each ares frame in the checkpoint's window and the closest one
is kept (its offset in frames is reported: a consistent offset is timing drift, not rendering). Both pictures are
blurred slightly first (see load()), so the VI filter and dither of ares do not count. Per checkpoint:
  score   mean absolute RGB difference, 0-255 (the ranking)
  bad%    pixels where some channel differs by more than 32
  bias    mean signed difference port - ares per channel (a uniform colour or brightness shift shows here)
  shift   the whole-picture offset (dx, dy within 4 pixels) that fits best, and how much of the score it removes
  detail  edge energy of the port relative to ares (< 1: blurrier or missing detail, > 1: extra or sharper detail)
  cells   the worst cells of an 8x6 grid (where on the screen the difference is)
  hints   short guesses from the numbers above (offset, colour shift, missing/extra content, blur, timing)
A 640x480 port picture (run.py's default) is compared field by field with ares's 240-line field (see load()).
Writes <report dir>/report.md and report.json (worst first), and <report dir>/cmp/<run>__<label>.png: port | ares |
difference (red where they differ, over a grey copy of ares), 2x size.
"""
import json, os, sys

import numpy as np
from PIL import Image, ImageFilter

W, H = 320, 240
BAD = 32
GRID_X, GRID_Y = 8, 6


BLUR = 1.0  # see load()


def load(path, blur=True, field=None):
    """The picture at 320x240. With blur, also softened a little (Gaussian, radius BLUR): ares shows the frame through
    the VI's dither and horizontal resampling filter and the port does not, which alone makes every edge and every
    textured background differ by a few levels (about 10 points of score on every screen in the first run); the blur
    removes that floor and keeps what matters (missing or extra things, wrong colours, offsets).
    field 0 or 1 takes only the even or odd rows of a 480-line picture first: ares_ref writes one 240-line field of
    the N64's 480-line interlaced picture (Zero draws its menus and text at 640x480), so a 640x480 port picture is
    compared one field at a time (see fields()); halving it to 240 lines instead would blend two fields and smear the
    small font."""
    im = Image.open(path).convert("RGB")
    if field is not None and im.size[1] == 2 * H:
        im = Image.fromarray(np.asarray(im)[field::2])
    if im.size != (W, H):
        im = im.resize((W, H), Image.BOX)
    if blur and BLUR > 0:
        im = im.filter(ImageFilter.GaussianBlur(BLUR))
    return np.asarray(im, dtype=np.int16)


def fields(path):
    """The port picture as candidates to compare with an ares field: both fields of a 480-line picture (the port draws
    a full frame, ares shows the field of the moment, and which one that is changes from frame to frame), or the
    picture itself."""
    if Image.open(path).size[1] == 2 * H:
        return [(0, load(path, field=0)), (1, load(path, field=1))]
    return [(None, load(path))]


def edges(a):
    g = a.mean(axis=2)
    return np.abs(np.diff(g, axis=0)).sum() + np.abs(np.diff(g, axis=1)).sum()


def best_shift(p, a, r=4):
    best = (0, 0, np.abs(p - a).mean())
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if dx == 0 and dy == 0:
                continue
            ps = p[max(0, dy):H + min(0, dy), max(0, dx):W + min(0, dx)]
            as_ = a[max(0, -dy):H + min(0, -dy), max(0, -dx):W + min(0, -dx)]
            m = np.abs(ps - as_)[r:-r or None, r:-r or None].mean()
            if m < best[2]:
                best = (dx, dy, m)
    return best


def heat(p, a):
    d = np.abs(p - a).max(axis=2).astype(np.float32)
    grey = a.mean(axis=2, keepdims=True).repeat(3, axis=2) * 0.45
    t = np.clip(d / 96.0, 0, 1)[..., None]
    red = np.zeros_like(grey)
    red[..., 0] = 255
    red[..., 1] = 40
    out = grey * (1 - t) + red * t
    out[d <= 8] = grey[d <= 8]
    return out.astype(np.uint8)


def analyse(p, a):
    diff = np.abs(p - a)
    score = float(diff.mean())
    bad = float((diff.max(axis=2) > BAD).mean() * 100)
    bias = [float(x) for x in (p - a).reshape(-1, 3).mean(axis=0)]
    dx, dy, shifted = best_shift(p, a)
    ep, ea = edges(p), edges(a)
    detail = float(ep / ea) if ea > 0 else (1.0 if ep == 0 else 9.99)
    cells = []
    cw, ch = W // GRID_X, H // GRID_Y
    m = diff.max(axis=2) > BAD
    for gy in range(GRID_Y):
        for gx in range(GRID_X):
            cells.append((float(m[gy * ch:(gy + 1) * ch, gx * cw:(gx + 1) * cw].mean() * 100), gx, gy))
    cells.sort(reverse=True)
    hints = []
    if score > 1 and shifted < score * 0.6 and (dx or dy):
        hints.append("offset %+d,%+d px" % (dx, dy))
    if max(abs(b) for b in bias) > 6 and np.std(p - a) < 1.5 * max(abs(b) for b in bias) + 8:
        hints.append("colour shift %+.0f/%+.0f/%+.0f" % tuple(bias))
    pm, am = p.max(axis=2) < 12, a.max(axis=2) < 12
    if (pm & ~am).mean() > 0.05:
        hints.append("black where ares draws (%.0f%%)" % ((pm & ~am).mean() * 100))
    if (am & ~pm).mean() > 0.05:
        hints.append("drawn where ares is black (%.0f%%)" % ((am & ~pm).mean() * 100))
    if detail < 0.75:
        hints.append("less detail (%.2f)" % detail)
    elif detail > 1.33:
        hints.append("more detail (%.2f)" % detail)
    spread = sum(1 for c in cells if c[0] > 10)
    if bad > 1:
        hints.append("local (%d cells)" % spread if spread <= 6 else "whole screen (%d cells)" % spread)
    return dict(score=round(score, 2), bad=round(bad, 2), bias=[round(b, 1) for b in bias],
                shift=[dx, dy], shift_score=round(float(shifted), 2), detail=round(detail, 3),
                cells=[[gx, gy, round(v, 1)] for v, gx, gy in cells[:4] if v > 0], hints=hints)


def compare_run(run, cmp_dir):
    name = os.path.basename(os.path.normpath(run))
    shots = []
    for line in open(os.path.join(run, "route.txt")):
        p = line.split()
        if p[0] == "shot":
            shots.append((int(p[1]), p[2]))
    rows = []
    for k, (frame, label) in enumerate(shots):
        pp = os.path.join(run, "port", label + ".ppm")
        ad = os.path.join(run, "ares", label)
        row = dict(run=name, label=label, order=k, frame=frame)
        afiles = sorted(f for f in os.listdir(ad) if f.endswith((".ppm", ".png"))) if os.path.isdir(ad) else []
        if not os.path.exists(pp) or not afiles:
            row.update(score=None, missing="port" if not os.path.exists(pp) else "ares")
            rows.append(row)
            continue
        cands = fields(pp)
        best = None
        for f in afiles:
            a = load(os.path.join(ad, f))
            for fld, p in cands:
                s = float(np.abs(p - a).mean())
                if best is None or s < best[0]:
                    best = (s, f, a, fld, p)
        _, af, a, fld, p = best
        port_frame = int(open(os.path.join(run, "port", label + ".frame")).read())
        row.update(analyse(p, a))
        row["port_frame"] = port_frame
        row["ares_frame"] = int(af[2:-4])
        row["ares_offset"] = int(af[2:-4]) - 1 - frame  # ares frame n = route frame n - 1
        lo, hi = int(afiles[0][2:-4]), int(afiles[-1][2:-4])
        if int(af[2:-4]) in (lo, hi) and row["score"] > 2:
            row["hints"].append("best ares frame at the window's edge: timing?")
        p0, a0 = load(pp, blur=False, field=fld), load(os.path.join(ad, af), blur=False)
        img = np.concatenate([p0.astype(np.uint8), a0.astype(np.uint8), heat(p, a)], axis=1)
        out = os.path.join(cmp_dir, "%s__%s.png" % (name, label))
        Image.fromarray(img).resize((img.shape[1] * 2, img.shape[0] * 2), Image.NEAREST).save(out, optimize=True)
        row["picture"] = os.path.relpath(out, os.path.dirname(cmp_dir))
        rows.append(row)
    return rows


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    rep = os.path.abspath(sys.argv[1])
    cmp_dir = os.path.join(rep, "cmp")
    os.makedirs(cmp_dir, exist_ok=True)
    rows = []
    for run in sys.argv[2:]:
        rows += compare_run(run, cmp_dir)
    ok = sorted((r for r in rows if r.get("score") is not None), key=lambda r: -r["score"])
    missing = [r for r in rows if r.get("score") is None]
    json.dump(ok + missing, open(os.path.join(rep, "report.json"), "w"), indent=1, ensure_ascii=False)
    md = ["# Port vs ares: %d checkpoints\n" % len(rows),
          "Score = mean absolute RGB difference at 320x240 after a slight blur (0 = identical). Pictures: port | ares | difference.\n",
          "| # | checkpoint | score | bad% | frames off | bias R/G/B | detail | hints |",
          "|---|---|---|---|---|---|---|---|"]
    for i, r in enumerate(ok, 1):
        md.append("| %d | [%s / %s](%s) | %.1f | %.1f | %+d | %s | %.2f | %s |" % (
            i, r["run"], r["label"], r["picture"], r["score"], r["bad"], r["ares_offset"],
            "/".join("%+.0f" % b for b in r["bias"]), r["detail"], "; ".join(r["hints"])))
    for r in missing:
        md.append("| - | %s / %s | no %s picture | | | | | |" % (r["run"], r["label"], r["missing"]))
    open(os.path.join(rep, "report.md"), "w").write("\n".join(md) + "\n")
    print("%d checkpoints compared, %d missing -> %s" % (len(ok), len(missing), os.path.join(rep, "report.md")))
    for r in ok[:10]:
        print("  %6.1f  %s / %s  %s" % (r["score"], r["run"], r["label"], "; ".join(r["hints"])))


if __name__ == "__main__":
    main()
