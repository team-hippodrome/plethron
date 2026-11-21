"""model_only.py <battle dir, e.g. pass1/b19> <turns, e.g. 11,12> <label> <out.route>

Short routes for 1P moves that play no move effect script (Withdraw on Blastoise/Cloyster, Drill Peck on Zapdos:
BattleScene_TickMoveStartEffect skips the script, the Pokémon's own model animation is the whole move). Replays a pass-1
battle up to the command phase after the chosen turns, with checkpoints after 1P's move event (anchored by the
command-phase sync before it)."""
import json, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "screen_diff"))
import run as sd  # screen_diff run.py
W = os.environ.get("MOVE_COVERAGE_WORK", "/home/user/mc4852")
OFFS = (2, 30, 60, 90, 120, 160, 200, 240)

def make(src, turns, name, out):
    bdir = os.path.join(W, src)
    p = json.load(open(os.path.join(bdir, "play.json")))
    menus = os.path.join(bdir, "menus.route")
    header = "".join(l for l in open(menus) if l.startswith("@"))
    prefix = [tuple(x) for x in sd.parse_route(menus)["presses"]]
    ev = p["events"]
    cmds = [e[0] for e in ev if e[1] == "cmd"]
    last = cmds[max(turns)]  # the command phase after the last chosen turn (turns are 1-based)
    items = []
    for f, spec, hold in prefix + [tuple(x) for x in p["presses"]]:
        if f < last:
            items.append((f, 2, "hold:%s:%d" % (spec, hold), hold + 1))
    started = False
    for f, kind, side, move in ev:
        if f > last:
            break
        if kind == "cmd":
            items.append((f, 0, "sync:cmd", 0)); started = True
        elif kind in ("anim", "charge") and side == 0 and started:
            items.append((f, 0, "sync:anim1", 0))
    for t in turns:
        c0, c1 = cmds[t - 1], cmds[t]
        mv = [e[0] for e in ev if e[1] == "move" and c0 < e[0] < c1]
        # 1P's move: the move event that no COM animation follows before the next one
        coms = [e[0] for e in ev if e[1] == "anim" and e[2] == 1 and c0 < e[0] < c1]
        mine = [m for i, m in enumerate(mv) if not any(m < a < (mv[i + 1] if i + 1 < len(mv) else c1) for a in coms)]
        f0 = mine[0]
        for d in OFFS:
            items.append((f0 + d, 1, "shot:t%02d_%s_%d" % (t, name, d), 0))
    items.sort(key=lambda x: (x[0], x[1]))
    toks, frame = [], 0
    for f, _, tok, length in items:
        if f > frame:
            toks.append(str(f - frame)); frame = f
        toks.append(tok); frame += length
    toks.append("120")
    with open(out, "w") as fo:
        fo.write("# model-only move check: %s turns %s of %s\n" % (name, turns, src) + header + " ".join(toks) + "\n")

if __name__ == "__main__":
    make(sys.argv[1], [int(x) for x in sys.argv[2].split(",")], sys.argv[3], sys.argv[4])
