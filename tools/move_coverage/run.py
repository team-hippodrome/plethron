#!/usr/bin/env python3 -I
"""Every Pokémon Stadium Zero can battle with (40 species) using every move it can have (Zero's own legality check),
played on the port and on ares and compared picture by picture.

Usage:
  tools/move_coverage/run.py plan <work> [--pass N]       moveset dump (from your ROM), test teams, saves, menu routes
  tools/move_coverage/run.py play <work> [--battles A-B] [--jobs N]   the port plays each battle with the planner and
                                                          writes <work>/pass<N>/b<NN>/battle.route
  tools/move_coverage/run.py route <work> [--battles A-B] rewrites battle.route from play.json (no replay)
  tools/move_coverage/run.py diff <work> [--battles A-B] [--jobs N] [--keep-frames]
                                                          each battle route on the port and on ares, compared
                                                          (tools/screen_diff); ares's frames are deleted afterwards
  tools/move_coverage/run.py report <work>                <work>/coverage.md: species x move, done / not reached,
                                                          with the worst picture differences per move
  tools/move_coverage/run.py all <work> [--jobs N]        plan, play, diff and report, then a second pass with the
                                                          moves that were not reached (other teams and battles)

How it plays (tools/move_coverage/planner.py): 1P enters a registered team of six test Pokémon, each knowing up to
four of its moves (an enabler such as Substitute for healing moves or a sleep move for Dream Eater rides along where
there is room). The COM enters a passive team (Chansey, Snorlax, Vaporeon, Blastoise, Lapras, Cloyster at L100 with
only self-boosting and healing moves), so nothing the COM does gets in the way. The planner picks 1P's moves from the
battle state the port reports at each command phase until every test move has started its animation (or failed four
times), Rage last (it never ends), and stops there. The input it gave becomes a tools/screen_diff route with seeded
random numbers (@rng) and a sync point at every command phase and move, which replays the same battle on ares.

The registered teams live in a crafted Zero save (tools/move_coverage/zero_teams.py): no Game Boy cartridge is needed.
The species and move data come from the port reading your ROM (PORT_DUMP_MOVESETS, src/port/move_coverage.c), into
<work>; nothing from the ROM goes into the repository, and the pictures stay in <work>.

Environment: as tools/screen_diff/run.py (ROM, PORT_BIN, ARES_REF_BUILD, ARES_REF_SRC; DISPLAY for the hidden port
window on Linux).
"""
import json, math, os, shutil, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
from types import SimpleNamespace

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "screen_diff"))
import planner  # noqa: E402
from planner import sd  # tools/screen_diff/run.py  # noqa: E402
import zero_teams as zt  # noqa: E402
from move_names import MOVE_NAMES, SPECIES_NAMES  # noqa: E402

SEED = "1"
TEAM_SIZE = 6
BATTLES_PER_SAVE = 15  # registration slot 0 holds the COM's team
# the COM's team: (dex, moves); only moves that do nothing to 1P (no status, no stat drops, no evasion or Mist,
# which would make 1P's moves fail), Water and Normal types (nothing of 1P's but Ghost moves on Normal is immune)
COM_TEAM = [(113, [111, 113, 115, 135]), (143, [106, 115, 133]), (134, [115, 116, 151]), (9, [110, 115]),
            (131, [115]), (91, [110, 115])]
SHOTS = (2, 30, 60, 90, 120)  # checkpoints after each 1P move's sync point (route frames)
WINDOW = (6, 40)  # ares frames kept around each checkpoint (compare.py picks the closest)


def port_bin():
    return os.path.abspath(os.environ.get("PORT_BIN", os.path.join(ROOT, "build", "plethron")))


# ---- data ----------------------------------------------------------------------------------------------------------
def dump(work):
    path = os.path.join(work, "movesets.txt")
    if not os.path.exists(path):
        env = dict(os.environ, PORT_DUMP_MOVESETS=path, SDL_AUDIO_DRIVER="dummy")
        env.setdefault("SDL_VIDEO_DRIVER", "dummy")
        subprocess.run([port_bin(), os.environ["ROM"]], env=env, cwd=os.path.dirname(port_bin()),
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=300)
        if not os.path.exists(path):
            sys.exit("the port wrote no moveset dump (PORT_DUMP_MOVESETS)")
    moves, species, legal = {}, {}, {}
    for line in open(path):
        p = line.split()
        if not p or p[0].startswith("#"):
            continue
        v = [int(x) for x in p[1:]]
        if p[0] == "M":
            moves[v[0]] = tuple(v[1:])
        elif p[0] == "S":
            species[v[0]] = dict(dex=v[0], gen1=v[1], flags=v[2], hp=v[3], atk=v[4], dfn=v[5], spd=v[6], spc=v[7],
                                 type1=v[8], type2=v[9], catch=v[10], growth=v[11])
        elif p[0] == "L":
            legal.setdefault(v[0], {})[v[1]] = v[2]
    return moves, species, legal


# ---- teams ---------------------------------------------------------------------------------------------------------
def chunks_for(dex, wanted, legal, moves):
    """The species' wanted moves in groups of up to four, with enablers added where there is room."""
    eff = lambda m: moves[m][0]
    can = legal[dex]
    sleep = [m for m in can if eff(m) == planner.SLEEP]
    sub = [m for m in can if eff(m) == planner.SUBSTITUTE]
    recoil = [m for m in can if eff(m) == 0x30]
    rest = sorted(wanted, key=lambda m: (eff(m) == planner.RAGE, m))
    out = []
    # moves that need another move first get their own group with it
    for m in [m for m in rest if eff(m) == planner.DREAM_EATER]:
        rest.remove(m)
        g = [m] + [s for s in sleep if s in rest][:1]
        if len(g) == 1 and sleep:
            g.append(sleep[0])
        out.append(g)
    for m in [m for m in rest if eff(m) == planner.HEAL]:
        rest.remove(m)
        en = [s for s in sub + recoil if s in rest][:1] or (sub + recoil)[:1]
        g = [m] + en
        for e in en:
            if e in rest:
                rest.remove(e)
        out.append(g)
    while rest:
        # fill the open places of the special groups first, then new groups of four
        g = next((g for g in out if len(g) < 4), None)
        if g is None:
            g = []
            out.append(g)
        g.append(rest.pop(0))
    return [sorted(set(g), key=g.index) for g in out]


def build_teams(wanted, legal, moves, species):
    """wanted: {dex: [move]} -> battles: [[{dex, moves, wanted, level}]]"""
    groups = []
    for dex in sorted(wanted, key=lambda d: -len(wanted[d])):
        for g in chunks_for(dex, wanted[dex], legal, moves):
            need = max(legal[dex][m] for m in g)
            level = 100 if any(moves[m][0] == planner.OHKO for m in g) else max(50, need)
            groups.append(dict(dex=dex, moves=g, wanted=[m for m in g if m in wanted[dex]], level=level))
    # one Rage user per battle at most: Rage never ends, so the battle stops at the first one
    rages = sum(1 for g in groups if any(moves[m][0] == planner.RAGE for m in g["moves"]))
    n = max(1, math.ceil(len(groups) / TEAM_SIZE), rages,
            max((sum(1 for g in groups if g["dex"] == d) for d in wanted), default=1))
    battles = [[] for _ in range(n)]
    status = (planner.SLEEP, planner.POISON, planner.PARALYZE, planner.CONFUSE, planner.LEECH_SEED)

    def cost(b, g):
        rage = any(moves[m][0] == planner.RAGE for m in g["moves"])
        st = sum(1 for m in g["moves"] if moves[m][0] in status)
        return (rage and any(moves[m][0] == planner.RAGE for x in b for m in x["moves"]),
                len(b),
                st + sum(1 for x in b for m in x["moves"] if moves[m][0] in status))
    # the species with the most groups first, each group into the emptiest battle without that species
    for g in sorted(groups, key=lambda g: -sum(1 for x in groups if x["dex"] == g["dex"])):
        free = [b for b in battles if len(b) < TEAM_SIZE and all(x["dex"] != g["dex"] for x in b)]
        if not free:
            battles.append([])
            free = [battles[-1]]
        min(free, key=lambda b: cost(b, g)).append(g)
    battles = [b for b in battles if b]
    # a battle with fewer than six (a later pass) is filled up with helpers: Free Battle's order screen takes six,
    # and a helper's strong moves knock the COM's Pokémon out when nothing else is left to do
    def strength(d):
        return sorted((m for m in legal[d] if moves[m][1] > 1 and moves[m][0] not in (planner.EXPLODE, planner.RAGE,
                       planner.OHKO)), key=lambda m: -moves[m][1])[:4]
    helpers = sorted(legal, key=lambda d: -sum(moves[m][1] for m in strength(d)))
    for b in battles:
        for d in helpers:
            if len(b) >= TEAM_SIZE:
                break
            if all(x["dex"] != d for x in b) and strength(d):
                ms = strength(d)
                b.append(dict(dex=d, moves=ms, wanted=[], level=max(50, max(legal[d][m] for m in ms))))
    return battles


def mon_record(sp, g, moves, ot_id, nick, com=False):
    pps = [moves[m][4] for m in g["moves"]]
    if com:
        return zt.box_struct(sp, 100, g["moves"], pps, ot_id, dvs=(15, 15, 0, 15), statexp=(65535, 0, 65535, 0, 65535),
                             pp_ups=3), nick
    return zt.box_struct(sp, g["level"], g["moves"], pps, ot_id, pp_ups=3), nick


def menu_route(sram, k):
    """Boot to the first command phase: Free Battle, 1P vs COM, 1P enters registered team k+1, the COM team 0, every
    Pokémon in party order on the order screen."""
    downs = " ".join(["DOWN 53"] * (k + 1))
    ups = " ".join(["UP 53"] * (k + 1))  # (the COM's cursor starts on 1P's entry)
    return ("@sram %s\n@rng %s\n@window %d %d\n" % (sram, SEED, WINDOW[0], WINDOW[1]) +
            "1200 START 300 A 400 A 150 A 150 A 150 A 150 A 200 A 200 %s A 200 A 200 %s A 200 A 400\n" % (downs, ups) +
            "1300 B 40 CL 40 CU 40 A 40 CD 40 CR 60 A\n")


def cmd_plan(work, npass=1):
    moves, species, legal = dump(work)
    pdir = os.path.join(work, "pass%d" % npass)
    if npass == 1:
        wanted = {d: sorted(legal[d]) for d in legal}
    else:
        cov = coverage(work)
        wanted = {}
        for (d, m), v in cov.items():
            if v["state"] != "done":
                wanted.setdefault(d, []).append(m)
        if not wanted:
            print("every move is done")
            return
    battles = build_teams(wanted, legal, moves, species)
    os.makedirs(pdir, exist_ok=True)
    com = [mon_record(species[d], dict(moves=ms), moves, 4321, zt.code_name(i + 25 * 24), com=True)
           for i, (d, ms) in enumerate(COM_TEAM)]
    plan = []
    for s in range(0, len(battles), BATTLES_PER_SAVE):
        sram = os.path.join(pdir, "teams%02d.sra" % (s // BATTLES_PER_SAVE))
        teams = [("コノ", 4321, com)]
        for k, b in enumerate(battles[s:s + BATTLES_PER_SAVE]):
            mons = [mon_record(species[g["dex"]], g, moves, 1234, zt.code_name(len(plan) * TEAM_SIZE + i))
                    for i, g in enumerate(b)]
            teams.append(("サトシ", 1234, mons))
            bdir = os.path.join(pdir, "b%02d" % len(plan))
            os.makedirs(bdir, exist_ok=True)
            open(os.path.join(bdir, "menus.route"), "w").write(menu_route(sram, k))
            plan.append(dict(battle=len(plan), dir=os.path.relpath(bdir, work), team=b))
        zt.write_sram(sram, teams)
    json.dump(plan, open(os.path.join(pdir, "plan.json"), "w"), indent=1)
    print("pass %d: %d test Pokémon in %d battles, %d species-move pairs -> %s" % (
        npass, sum(len(b) for b in battles), len(battles), sum(len(v) for v in wanted.values()), pdir))


# ---- play / diff ---------------------------------------------------------------------------------------------------
def load_plan(work, npass):
    return json.load(open(os.path.join(work, "pass%d" % npass, "plan.json")))


def pick(plan, spec):
    if not spec:
        return plan
    lo, _, hi = spec.partition("-")
    return [b for b in plan if int(lo) <= b["battle"] <= int(hi or lo)]


def write_battle_route(bdir, b, battle):
    """battle.route from the planner's battle (or one rebuilt from play.json): the menu route's settings and presses,
    then the battle's presses, sync points and checkpoints."""
    menus = os.path.join(bdir, "menus.route")
    header = "".join(l for l in open(menus) if l.startswith("@"))
    header = "# move coverage battle %d (tools/move_coverage): %s\n" % (b["battle"], ", ".join(
        "%s (%s)" % (SPECIES_NAMES.get(g["dex"], g["dex"]), "/".join(MOVE_NAMES.get(m, str(m)) for m in g["moves"]))
        for g in b["team"])) + header

    def label(k):
        n, move, kind, d = k
        return "m%02d_%s_%s%d" % (n, MOVE_NAMES.get(move, str(move)).replace(" ", "").replace("-", ""),
                                  "c_" if kind == "charge" else "", d)
    prefix = getattr(battle, "prefix_presses", None)
    if prefix is None:
        prefix = list(sd.parse_route(menus)["presses"])
    planner.write_route(os.path.join(bdir, "battle.route"), header, prefix, battle, SHOTS, label)


def route_one(work, b):
    """Rewrites battle.route from play.json without playing again (after a change to the route format)."""
    bdir = os.path.join(work, b["dir"])
    pj = os.path.join(bdir, "play.json")
    if not os.path.exists(pj):
        return b["battle"], "no play.json (play first)"
    r = json.load(open(pj))
    battle = SimpleNamespace(events=[tuple(e) for e in r["events"]], presses=[tuple(p) for p in r["presses"]],
                             ended=r["ended"])
    write_battle_route(bdir, b, battle)
    return b["battle"], "battle.route rewritten"


def cmd_route(work, npass, spec):
    for b in pick(load_plan(work, npass), spec):
        print("battle %d: %s" % route_one(work, b), flush=True)


def play_one(work, b, moves):
    bdir = os.path.join(work, b["dir"])
    team = [dict(dex=g["dex"], moves=g["moves"], wanted=g["wanted"], level=g.get("level", 50)) for g in b["team"]]
    pend = {(i, m) for i, g in enumerate(b["team"]) for m in g["wanted"]}
    out = os.path.join(bdir, "plan_run")
    battle = planner.play(os.path.join(bdir, "menus.route"), team, moves, out, seed=SEED, log=lambda s: None,
                          pending=pend, com_species=[d for d, _ in COM_TEAM])
    if battle.wrong_com:
        return b["battle"], 0, 0, len(pend), 0
    write_battle_route(bdir, b, battle)
    res = dict(done=[[s, m, a] for (s, m), a in battle.done.items()],
               gave_up=sorted([s, m] for (s, m), n in battle.tries.items() if (s, m) not in battle.done),
               not_reached=sorted([s, m] for (s, m) in battle.pending),
               turns=battle.turn, ended=battle.ended, surrendered=battle.surrendered,
               events=[[f, k, s, m] for f, k, s, m in battle.events], presses=battle.presses)
    json.dump(res, open(os.path.join(bdir, "play.json"), "w"), indent=1)
    shutil.rmtree(out + "_home", ignore_errors=True)
    return b["battle"], len(battle.done), len(res["gave_up"]), len(res["not_reached"]), battle.turn


def cmd_play(work, npass, spec, jobs):
    moves = dump(work)[0]
    plan = pick(load_plan(work, npass), spec)
    with ThreadPoolExecutor(jobs) as ex:
        for r in ex.map(lambda b: play_one(work, b, moves), plan):
            print("battle %d: %d done, %d not animated, %d not reached, %d turns" % r, flush=True)


def diff_one(work, b, keep):
    bdir = os.path.join(work, b["dir"])
    route = os.path.join(bdir, "battle.route")
    if not os.path.exists(route):
        return b["battle"], "no route (play first)"
    run = os.path.join(bdir, "screens")
    env = dict(os.environ)
    env.setdefault("SCREEN_DIFF_TIMEOUT", "21600")  # (a battle is 30,000-60,000 frames; ares on a software renderer is slow)
    p = subprocess.run([sys.executable, "-I", os.path.join(ROOT, "tools", "screen_diff", "run.py"), route, run],
                       capture_output=True, text=True, env=env)
    open(os.path.join(bdir, "screens.log"), "w").write(p.stdout + p.stderr)
    rep = os.path.join(bdir, "report")
    subprocess.run([sys.executable, "-I", os.path.join(ROOT, "tools", "screen_diff", "compare.py"), rep, run],
                   capture_output=True, text=True)
    if not keep:
        shutil.rmtree(os.path.join(run, "ares"), ignore_errors=True)
        shutil.rmtree(os.path.join(run, "port_home"), ignore_errors=True)
    sync = [l for l in p.stdout.splitlines() if "sync points" in l or "random numbers" in l]
    return b["battle"], "; ".join(sync)


def cmd_diff(work, npass, spec, jobs, keep):
    plan = pick(load_plan(work, npass), spec)
    with ThreadPoolExecutor(jobs) as ex:
        for n, msg in ex.map(lambda b: diff_one(work, b, keep), plan):
            print("battle %d: %s" % (n, msg), flush=True)


# ---- report --------------------------------------------------------------------------------------------------------
def coverage(work):
    """(dex, move) -> {state: done | not animated | not reached | not played, battle, scores}"""
    moves, species, legal = dump(work)
    cov = {(d, m): dict(state="not played") for d in legal for m in legal[d]}
    npass = 1
    while os.path.exists(os.path.join(work, "pass%d" % npass, "plan.json")):
        for b in load_plan(work, npass):
            bdir = os.path.join(work, b["dir"])
            pj = os.path.join(bdir, "play.json")
            if not os.path.exists(pj):
                continue
            res = json.load(open(pj))
            rep = os.path.join(bdir, "report", "report.json")
            rows = json.load(open(rep)) if os.path.exists(rep) else []
            # checkpoint labels m<n>_<move>_<frames>, n = the n-th 1P turn with an animation (planner.write_route).
            # The done moves are in the order they were done, so each takes the next turn that animated it (its own
            # animation, or what Metronome / Mirror Move called): two species of one battle with the same move get
            # their own turns, and turns that only repeat a move (to knock the COM out) are left out.
            turns = {}
            for r in rows:
                if r.get("score") is not None:
                    n, name = r["label"].split("_")[:2]
                    turns.setdefault(int(n[1:]), dict(name=name, rows=[]))["rows"].append(r)
            order, k = sorted(turns), 0
            norm = lambda m: MOVE_NAMES.get(m, str(m)).replace(" ", "").replace("-", "")
            for s, m, anims in res["done"]:
                key = (b["team"][s]["dex"], m)
                if key not in cov or cov[key]["state"] == "done":
                    continue
                cov[key] = dict(state="done", battle=b["dir"], anims=anims, scores=[])
                names = {norm(m)} | {norm(x) for _, x in anims}
                j = k
                while j < len(order) and turns[order[j]]["name"] not in names:
                    j += 1
                if j < len(order):
                    cov[key]["scores"] = [(r.get("lit", r["score"]), r["score"],
                                           os.path.join(b["dir"], "report", r["picture"])) for r in turns[order[j]]["rows"]]
                    k = j + 1
            for state, items in (("not animated", res["gave_up"]), ("not reached", res["not_reached"])):
                for s, m in items:
                    key = (b["team"][s]["dex"], m)
                    if key in cov and cov[key]["state"] in ("not played", "not reached"):
                        cov[key] = dict(state=state, battle=b["dir"])
        npass += 1
    return cov


def cmd_report(work):
    moves, species, legal = dump(work)
    cov = coverage(work)
    counts = {}
    for v in cov.values():
        counts[v["state"]] = counts.get(v["state"], 0) + 1
    md = ["# Move coverage: every battle Pokémon using every move\n",
          "%d species-move pairs (Zero's legality check, %d species): %s.\n" % (
              len(cov), len(legal), ", ".join("%d %s" % (n, s) for s, n in sorted(counts.items()))),
          "Lit: the worst port-vs-ares picture difference among the move's checkpoints once a smooth lighting"
          " difference is taken out (tools/screen_diff/compare.py lit_score(): the colosseum floor's lighting and an"
          " overall brightness offset differ from ares on every screen and would hide the moves that really differ;"
          " ~2-4 is the floor). Raw: the plain score of that checkpoint (~6-9 is the floor). A raw score far above"
          " lit means the difference is mostly smooth lighting or a wash over the screen.\n",
          "| species | move | state | lit | raw | picture |", "|---|---|---|---|---|---|"]
    for (d, m), v in sorted(cov.items()):
        worst = max(v.get("scores") or [(None, None, "")], key=lambda x: -1 if x[0] is None else x[0])
        md.append("| %s | %s | %s | %s | %s | %s |" % (
            SPECIES_NAMES.get(d, d), MOVE_NAMES.get(m, m), v["state"],
            "%.1f" % worst[0] if worst[0] is not None else "", "%.1f" % worst[1] if worst[1] is not None else "",
            "[cmp](%s)" % worst[2] if worst[2] else ""))
    open(os.path.join(work, "coverage.md"), "w").write("\n".join(md) + "\n")
    json.dump({"%d:%d" % k: v for k, v in cov.items()}, open(os.path.join(work, "coverage.json"), "w"), indent=1)
    print("%s -> %s" % (", ".join("%d %s" % (n, s) for s, n in sorted(counts.items())), os.path.join(work, "coverage.md")))


def main():
    a = sys.argv[1:]
    if len(a) < 2:
        sys.exit(__doc__)
    cmd, work = a[0], os.path.abspath(a[1])
    opt = lambda k, d=None: a[a.index(k) + 1] if k in a else d
    npass, jobs = int(opt("--pass", "1")), int(opt("--jobs", "2"))
    if "ROM" not in os.environ:
        sys.exit("set ROM to the Pocket Monsters Stadium (Japan) .z64")
    os.makedirs(work, exist_ok=True)
    if cmd == "plan":
        cmd_plan(work, npass)
    elif cmd == "play":
        cmd_play(work, npass, opt("--battles"), jobs)
    elif cmd == "route":
        cmd_route(work, npass, opt("--battles"))
    elif cmd == "diff":
        cmd_diff(work, npass, opt("--battles"), jobs, "--keep-frames" in a)
    elif cmd == "report":
        cmd_report(work)
    elif cmd == "all":
        for n in (1, 2):
            if n == 2:
                cmd_report(work)
            cmd_plan(work, n)
            if not os.path.exists(os.path.join(work, "pass%d" % n, "plan.json")):
                break
            cmd_play(work, n, None, jobs)
            cmd_diff(work, n, None, jobs, False)
        cmd_report(work)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
