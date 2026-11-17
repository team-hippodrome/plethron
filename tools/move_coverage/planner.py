#!/usr/bin/env python3 -I
"""Plays one Free Battle on the port, deciding 1P's input at each command phase from the battle state, and writes
what it pressed as a tools/screen_diff route: the same battle then replays on the port and on ares (random numbers
seeded alike with @rng, sync points at each command phase and each move).

Used by tools/move_coverage/run.py. The port side is src/port/os_devices.cpp (PORT_PLANNER_IN / PORT_PLANNER_OUT)
and src/port/move_coverage.c (the state lines, the animation events).

The strategy: each 1P Pokémon knows up to four test moves; a move is done once a 1P move animation starts in the turn
it was picked. Moves whose effect needs a condition wait until it holds (a status move needs a target without that
status, Dream Eater a sleeping target, a healing move missing HP, ...); when nothing on the field can be used, 1P
switches, and when no test move can be used at all it attacks with a move that is already done (to knock the COM's
Pokémon out: the next one has no status). Rage (which never ends) comes last, then 1P surrenders.
"""
import os, sys, threading

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "screen_diff"))
import run as sd  # tools/screen_diff/run.py  # noqa: E402

BUTTONS = sd.BUTTONS
MOVE_BUTTONS = ["CU", "CR", "CD", "CL"]  # move slots 0-3 (BattleMenu_HandleMoveButtons)
SWITCH_BUTTONS = ["CU", "CL", "B", "CR", "CD", "A"]  # party slots 0-5 with six (BattleMenu_HandleSwitchButtons)

# Gen 1 move effects (the move table's effect byte, pokered constants; tools/battle_sweep.py has the full list)
SLEEP, POISON, PARALYZE, CONFUSE, LEECH_SEED, DREAM_EATER = 0x20, 0x42, 0x43, 0x31, 0x54, 0x08
HEAL, EXPLODE, RAGE, OHKO, SUBSTITUTE, DISABLE, MIRROR_MOVE = 0x38, 0x07, 0x51, 0x26, 0x4F, 0x56, 0x09
METRONOME = 0x53
REFLECT, LIGHT_SCREEN, MIST, FOCUS_ENERGY, TRANSFORM, MIMIC = 0x41, 0x40, 0x2E, 0x2F, 0x39, 0x52
NORMAL, FIGHTING, FLYING, POISON_T, GROUND, GHOST, GRASS, ELECTRIC, PSYCHIC = 0, 1, 2, 3, 4, 8, 0x16, 0x17, 0x18
IMMUNE = {NORMAL: {GHOST}, FIGHTING: {GHOST}, GHOST: {NORMAL, PSYCHIC}, GROUND: {FLYING}, ELECTRIC: {GROUND}}
GIVE_UP = 4  # tries before a test move counts as not reachable in this setup (OHKO moves: 10)


def parse_state(lines):
    """-> {side: {field: value}} with 'roster': [(species, hp, status)]"""
    st = {0: {"roster": []}, 1: {"roster": []}}
    for l in lines:
        p = l.split()
        if p[0] == "S":
            d = st[int(p[1])]
            for kv in p[2:]:
                k, v = kv.split("=")
                d[k] = [int(x) for x in v.split(",")] if "," in v else int(v)
        elif p[0] == "R":
            st[int(p[1])]["roster"].append((int(p[3]), int(p[4]), int(p[5])))
    return st


class Battle:
    """team: [{'dex', 'moves': [ids], 'nick'}] in party order; moves: {id: (effect, power, type, acc, pp)}."""

    def __init__(self, team, moves, log):
        self.team, self.moves, self.log = team, moves, log
        self.pending = {(i, m) for i, mon in enumerate(team) for m in mon["moves"]}
        self.tries = {}
        self.done = {}  # (slot, move) -> list of (kind, move) animations seen
        self.presses = []  # (frame, spec, hold)
        self.events = []  # (frame, kind, ...) in order
        self.turn = 0
        self.picked = None  # (slot, move, frame) of the current turn
        self.picked_asleep = False
        self.com_species = None
        self.wrong_com = False
        self.turn_anims = []
        self.surrendered = False
        self.ended = None
        self.started = False
        self.cmd_frames = []

    # ---- bookkeeping ----
    def note(self, frame, kind, side=None, move=None):
        self.events.append((frame, kind, side, move))
        if kind in ("anim", "charge") and side == 0:
            self.turn_anims.append((kind, move))

    def close_turn(self):
        if self.picked is None:
            return
        slot, move, _ = self.picked
        key = (slot, move)
        ids = {m for _, m in self.turn_anims}
        # the picked move's own animation (Metronome and Mirror Move play the move they call)
        animated = move in ids or (self.moves.get(move, (0,))[0] in (METRONOME, MIRROR_MOVE) and bool(ids - {56}))
        if animated and key in self.pending:
            self.pending.discard(key)
            self.done[key] = list(self.turn_anims)
            self.log("  done: slot %d move %d (%s)" % (slot, move, self.turn_anims))
        elif key in self.pending and not self.picked_asleep:
            self.tries[key] = self.tries.get(key, 0) + 1
            limit = 10 if self.moves[move][0] == OHKO else GIVE_UP
            if self.tries[key] >= limit:
                self.pending.discard(key)
                self.log("  gave up: slot %d move %d after %d tries" % (slot, move, self.tries[key]))
        self.picked = None
        self.turn_anims = []

    def press(self, frame, spec, hold=6):
        if self.presses and frame <= self.presses[-1][0] + self.presses[-1][2]:
            frame = self.presses[-1][0] + self.presses[-1][2] + 1
        self.presses.append((frame, spec, hold))
        return ["p %d %d %d" % (frame, sd.mask(spec), hold)]

    # ---- decisions ----
    def usable(self, slot, move, me, foe):
        eff, power, mtype, _, _ = self.moves[move]
        ftypes = {foe["t"][0], foe["t"][1]}
        if power > 0 and (IMMUNE.get(mtype, set()) & ftypes):
            return False
        if eff in (SLEEP, POISON, PARALYZE) and foe["st"] != 0:
            return False
        if eff == POISON and POISON_T in ftypes:
            return False
        if eff == PARALYZE and mtype == ELECTRIC and GROUND in ftypes:
            return False
        if eff == CONFUSE and foe["conf"] != 0:
            return False
        if eff == LEECH_SEED and (foe["vol"] & 0x80 or GRASS in ftypes):
            return False
        if eff == DREAM_EATER and not (foe["st"] & 7):
            return False
        if eff == HEAL and me["hp"] >= me["mhp"]:
            return False
        if eff == SUBSTITUTE and (me["sub"] or me["hp"] * 4 <= me["mhp"]):
            return False
        if eff == RAGE:
            return False  # (only at the very end: rage_pick)
        return True

    def score(self, move, foe):
        eff = self.moves[move][0]
        if eff in (SLEEP, POISON, PARALYZE, CONFUSE, LEECH_SEED, DREAM_EATER):
            return 0  # needs a fresh target: first
        if eff == SUBSTITUTE:
            return 1  # makes room for the healing moves
        if eff == EXPLODE:
            return 9  # the user faints: last
        return 5

    def slot_moves(self, slot, me, foe):
        pps = me["pp"]
        out = []
        for i, m in enumerate(self.team[slot]["moves"]):
            if (slot, m) in self.pending and pps[i] & 0x3F > 0 and self.usable(slot, m, me, foe):
                out.append((self.score(m, foe), i, m))
        return sorted(out)

    def decide(self, st, frame):
        me, foe = st[0], st[1]
        active = me["ri"]
        roster = me["roster"]
        alive = [i for i, (sp, hp, _) in enumerate(roster) if hp > 0]
        if me["hp"] == 0 or active not in alive:
            # a replacement after a faint: the slot with the most pending moves
            choice = self.pick_switch(roster, alive, active, st, need=True)
            if choice is None:
                return None
            self.log("  send out slot %d" % choice)
            return ("switch", choice)
        if not any(k[0] in alive for k in self.pending):
            return ("surrender", None)
        cand = self.slot_moves(active, me, foe)
        if cand:
            _, idx, m = cand[0]
            return ("move", idx, m)
        # another Pokémon with something usable now (its own pending moves, judged against the same target)
        target = self.pick_switch(roster, alive, active, st)
        if target is not None:
            return ("switch", target)
        # nothing usable: attack with a done damaging move to get a fresh target, or Rage at the very end
        if all(k[1] in self.rage_moves() for k in self.pending if k[0] in alive):
            r = [(k[0], k[1]) for k in self.pending if k[0] in alive]
            slot, move = r[0]
            if slot != active:
                return ("switch", slot)
            return ("move", self.team[slot]["moves"].index(move), move)
        best = None
        for i, m in enumerate(self.team[active]["moves"]):
            eff, power, mtype = self.moves[m][:3]
            if power > 0 and me["pp"][i] & 0x3F and eff not in (EXPLODE, RAGE) and not (IMMUNE.get(mtype, set()) & set(foe["t"])):
                if best is None or power > best[0]:
                    best = (power, i, m)
        if best:
            return ("move", best[1], best[2])
        for i in alive:
            if i != active and any(self.moves[m][1] > 0 and self.moves[m][0] not in (EXPLODE, RAGE)
                                   for m in self.team[i]["moves"]):
                return ("switch", i)
        return ("surrender", None)

    def rage_moves(self):
        return {m for m, v in self.moves.items() if v[0] == RAGE}

    def pick_switch(self, roster, alive, active, st, need=False):
        best = None
        for i in alive:
            if i == active:
                continue
            n = sum(1 for k in self.pending if k[0] == i and self.moves[k[1]][0] != RAGE)
            if n and (best is None or n > best[0]):
                best = (n, i)
        if best:
            return best[1]
        if need:
            rest = [i for i in alive if i != active]
            return rest[0] if rest else None
        return None

    def on_cmd(self, frame, lines):
        st = parse_state(lines)
        self.close_turn()
        self.started = True
        self.turn += 1
        self.cmd_frames.append(frame)
        me = st[0]
        self.log("cmd %d turn %d: 1P sp=%s hp=%s/%s st=%s opt=%s cmd=%s imm=%s pp=%s | COM sp=%s hp=%s/%s st=%s" % (
            frame, self.turn, me.get("sp"), me.get("hp"), me.get("mhp"), me.get("st"), me.get("opt"), me.get("cmd"),
            me.get("imm"), me.get("pp"), st[1].get("sp"), st[1].get("hp"), st[1].get("mhp"), st[1].get("st")))
        if self.turn == 1 and self.com_species and st[1].get("sp") not in self.com_species:
            self.log("  the COM sent out species %s, not one of its team: the menus went wrong" % st[1].get("sp"))
            self.wrong_com = True
            return ["stop %d" % (frame + 1)]
        if self.surrendered:
            return []
        # look again once the command menu is up: a locked Pokémon (charging, recharging, thrashing, raging...) has
        # its action committed by then and takes no input
        return ["poll %d" % (frame + 170)]

    def on_poll(self, frame, lines):
        st = parse_state(lines)
        me = st[0]
        if me.get("opt") == 3 and not self.surrendered and not any(
                k[0] in [i for i, r in enumerate(me["roster"]) if r[1] > 0] for k in self.pending):
            self.log("  poll %d: locked and nothing left: surrender" % frame)
            self.surrendered = True
            return self.press(frame + 10, "START") + self.press(frame + 50, "RIGHT") + self.press(frame + 90, "A")
        if me.get("opt") == 3:
            self.log("  poll %d: 1P's action is committed (locked)" % frame)
            slot = me["ri"]
            if self.picked is None:
                self.picked = (slot, -1, frame)
            return []
        d = self.decide(st, frame)
        self.log("  poll %d: %s" % (frame, d))
        out = []
        if d is None:
            return out
        if d[0] == "move":
            _, idx, m = d
            self.picked = (me["ri"], m, frame)
            self.picked_asleep = bool(me["st"] & 0x27)  # asleep or frozen: the move cannot happen this turn
            out += self.press(frame + 10, "A")
            out += self.press(frame + 50, MOVE_BUTTONS[idx])
        elif d[0] == "switch":
            if me["hp"] == 0:
                out += self.press(frame + 10, SWITCH_BUTTONS[d[1]])
            else:
                out += self.press(frame + 10, "B")
                out += self.press(frame + 50, SWITCH_BUTTONS[d[1]])
        elif d[0] == "surrender":
            self.surrendered = True
            out += self.press(frame + 10, "START")
            out += self.press(frame + 50, "RIGHT")
            out += self.press(frame + 90, "A")
        return out

    def on_idle(self, frame):
        if not self.started:
            return []
        self.close_turn()
        if self.ended is None:
            self.ended = frame
            self.log("idle %d: the battle is over" % frame)
            return ["stop %d" % (frame + 60)]
        return []


def play(prefix_route, team, moves, out_dir, seed="1", log=print, extra_env=None, pending=None, com_species=None):
    """Runs the port with the planner. -> the Battle (presses, events, done)."""
    r = sd.parse_route(prefix_route)
    r["rng"] = r["rng"] or seed
    r["shots"], r["port_shots"] = [], []
    r["end"] = 10 ** 7  # (the planner stops the run)
    os.makedirs(out_dir, exist_ok=True)
    fin, fout = os.path.join(out_dir, "plan_in"), os.path.join(out_dir, "plan_out")
    for f in (fin, fout):
        if os.path.exists(f):
            os.remove(f)
        os.mkfifo(f)
    logf = open(os.path.join(out_dir, "planner.log"), "w")

    def L(s):
        logf.write(s + "\n")
        logf.flush()
        log(s)

    b = Battle(team, moves, L)
    b.com_species = set(com_species or ())
    if pending is not None:
        b.pending = set(pending)  # (the moves to cover; the others are enablers)
    b.prefix_presses = list(r["presses"])
    env = dict(extra_env or {}, PORT_PLANNER_IN=fin, PORT_PLANNER_OUT=fout, PORT_PLANNER_IDLE="1500")

    def loop():
        with open(fout) as rd, open(fin, "w") as wr:
            pending = []
            for line in rd:
                line = line.rstrip("\n")
                if line != "?":
                    pending.append(line)
                    p = line.split()
                    if p[0] in ("move", "anim", "charge"):
                        b.note(int(p[1]), p[0], *(int(x) for x in p[2:]))
                        pending.pop()
                    continue
                head, rest = pending[0].split(), pending[1:]
                pending = []
                kind, frame = head[0], int(head[1])
                if kind == "cmd":
                    b.note(frame, "cmd")
                    reply = b.on_cmd(frame, rest)
                elif kind == "poll":
                    reply = b.on_poll(frame, rest)
                else:
                    reply = b.on_idle(frame)
                wr.write("".join(x + "\n" for x in reply) + "ok\n")
                wr.flush()

    t = threading.Thread(target=loop, daemon=True)
    t.start()
    sd.run_port(r, out_dir, env)
    t.join(timeout=10)
    b.close_turn()
    return b


def write_route(path, header, prefix_presses, battle, shots_per_move=(2, 30, 60, 90, 120),
                label=lambda k: "m%02d_%d%s_%d" % (k[0], k[1], "c" if k[2] == "charge" else "", k[3])):
    """The replayable route: the prefix's presses, then each command phase and move as a sync point, 1P's presses,
    and checkpoints after each 1P move (by its sync:move)."""
    items = []  # (frame, order, token)
    for f, spec, hold in prefix_presses + battle.presses:
        items.append((f, 2, "hold:%s:%d" % (spec, hold), hold + 1))
    # sync points: every cmd and move event, in order; 1P's move = the last move event before a 1P anim/charge
    last_move = None
    shots = []
    for f, kind, side, move in battle.events:
        if kind == "cmd":
            items.append((f, 0, "sync:cmd", 0))
        elif kind == "move":
            items.append((f, 0, "sync:move", 0))
            last_move = f
        elif kind in ("anim", "charge") and side == 0 and last_move is not None:
            shots.append((last_move, move, kind))
            last_move = None
    n = 0
    for f0, move, kind in shots:
        n += 1
        for d in shots_per_move:
            items.append((f0 + d, 1, "shot:%s" % label((n, move, kind, d)), 0))
    items.sort(key=lambda x: (x[0], x[1]))
    out, frame = [], 0
    for f, _, tok, length in items:
        if f > frame:
            out.append(str(f - frame))
            frame = f
        elif f < frame and length:
            raise SystemExit("route: press at %d overlaps (now %d)" % (f, frame))
        out.append(tok)
        frame += length
    end = (battle.ended or frame) + 60
    if end > frame:
        out.append(str(end - frame))
    with open(path, "w") as fo:
        fo.write(header)
        line = []
        for tok in out:
            line.append(tok)
            if tok.startswith("sync:cmd"):
                fo.write(" ".join(line[:-1]) + "\n")
                line = [tok]
        fo.write(" ".join(line) + "\n")
