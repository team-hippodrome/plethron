#!/usr/bin/env python3
# Checks every translated move name against each C-button slot of the screens that lay moves out around the C cross,
# with the glyph sizing of src/port/unicode_font.cpp (as tools/check_label_fit.py) and the squish rule of
# Port_Font_FitNext (game/src/font.c): over by two average characters or less is squeezed, more is OVER (for the
# human localizers; AI translations are never shortened).
# usage: check_move_slots.py <build/assets/font/unifont_subset.hex> translations/ai.csv [translations/human.csv]
# Slot widths are the Port_Font_FitNext boxes in the code (font set heights from Font_Init: set 8 = 20, set 1 = 10):
#   rental / PC release card  free_battle_setup.c, pc_box.c (D_82009040 / D_8900A884: slots x 57, 245, 221, 33)
#   Organize stats footer     pc_sort.c (D_8900A79C: 219, 403, 379, 195; 552-wide footer)
#   party moves page          party_screen.c (D_8A203384: 208-wide boxes)
#   battle move panel         menu_panels.c (BattleMenu_DrawMovePanel: 200-wide panel, 8 px borders)
import csv, sys

hexf, aif = sys.argv[1], sys.argv[2]
humanf = sys.argv[3] if len(sys.argv) > 3 else None
W = {}
for l in open(hexf):
    if ':' in l and not l.startswith('#'):
        a, b = l.strip().split(':')
        W[int(a, 16)] = 8 if len(b) == 32 else 16


def adv(c, h):
    w = W.get(ord(c), W.get(ord('?'), 8)); sw = w + 2; s = h / 18
    return max(1, round(sw * s + 1e-9) if (sw * s) % 1 != 0.5 else int(sw * s) + 1) - (int(s) + 1 if s % 1 >= 0.5 else int(s))


def fit(t, h, avail):
    width = sum(adv(c, h) for c in t)
    n = len(t)
    return width, 'fits' if width <= avail else 'squish' if (width - avail) * n <= 2 * width else 'OVER'


# screen: font height, {slot: available width}
SCREENS = {
    'rental/PC card': (20, {'up': 197 - 8, 'left': 173 - 8, 'right': 410 - 245, 'down': 410 - 221}),
    'Organize footer': (20, {'up': 359 - 130, 'left': 335 - 130, 'right': 544 - 403, 'down': 544 - 379}),
    'party moves': (20, {'up': 225 - 91, 'left': 192 - 58, 'right': 494 - 332, 'down': 461 - 299}),
    'battle panel': (10, {'up': 90 - 8, 'left': 76 - 8, 'right': 192 - 124, 'down': 192 - 110}),
}
LANGS = ['ja', 'en', 'es', 'de', 'pt', 'ko']

rows = {}
for row in csv.DictReader(open(aif, encoding='utf-8')):
    if row['id'].endswith('_MOVE_NAME'):
        rows[row['id']] = dict(row)
if humanf:
    for row in csv.DictReader(open(humanf, encoding='utf-8')):
        if row['id'] in rows:
            for L in LANGS:
                if row.get(L):
                    rows[row['id']][L] = row[L]

count = {'squish': 0, 'OVER': 0}
for i, row in sorted(rows.items()):
    for L in LANGS:
        t = row[L]
        if not t:
            continue
        for screen, (h, slots) in SCREENS.items():
            for slot, avail in slots.items():
                wd, r = fit(t, h, avail)
                if r != 'fits':
                    count[r] += 1
                    print('%s\t%s\t%s\t%s\t%s\t%d/%d' % (r, screen, slot, i, L + ' ' + t, wd, avail))
print('# %d move names x %d languages: %d squeezed, %d over' % (len(rows), len(LANGS), count['squish'], count['OVER']),
      file=sys.stderr)
