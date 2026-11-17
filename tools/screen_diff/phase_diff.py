#!/usr/bin/env python3
"""Compares how long each battle scene state lasts in the port and in ares.

Usage: phase_diff.py <port_phase.txt> <ares_phase.txt>

Both logs have one line per change of the battle scene's state (BattleScene_UpdatePhase: mode, phase, step, substep,
acting side, move effect kind) with the frame it changed on: the port's PORT_PHASELOG, ares_ref's ARES_REF_PHASELOG
(run.py sets both for @rng routes). With the same random numbers both games go through the same states, so a state
that lasts longer on one side is where the two drift apart: a wait for an animation, a message, a camera shot.
"""
import difflib
import sys


def load(path):
    rows = [l.split() for l in open(path) if l.startswith("P ")]
    return [(int(r[1]), tuple(int(v) for v in r[2:8])) for r in rows]


def durations(rows):
    return [(rows[i][1], rows[i + 1][0] - rows[i][0]) for i in range(len(rows) - 1)]


def report(port_path, ares_path, out=sys.stdout):
    port, ares = durations(load(port_path)), durations(load(ares_path))
    match = difflib.SequenceMatcher(None, [s for s, _ in port], [s for s, _ in ares], autojunk=False)
    same = differ = 0
    lines = []
    for tag, i1, i2, j1, j2 in match.get_opcodes():
        if tag != "equal":
            if i2 > i1 and j2 > j1:
                lines.append("  states differ: port %s, ares %s" % ([s for s, _ in port[i1:i2]][:3], [s for s, _ in ares[j1:j2]][:3]))
            continue
        for k in range(i2 - i1):
            state, dp = port[i1 + k]
            da = ares[j1 + k][1]
            if dp == da:
                same += 1
                continue
            differ += 1
            lines.append("  mode %d phase %d step %d substep %d side %d kind %d: port %d frames, ares %d" % (state + (dp, da)))
    print("battle states: %d last as long on both sides, %d differ" % (same, differ), file=out)
    for l in lines[:20]:
        print(l, file=out)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    report(sys.argv[1], sys.argv[2])
