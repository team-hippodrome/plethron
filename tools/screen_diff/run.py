#!/usr/bin/env python3 -I
"""Plays one route on the port and on ares with the same input and saves the pictures at its checkpoints.

Usage: tools/screen_diff/run.py <route file> <out dir> [--port-only | --ares-only] [--window B,A] [--port-env K=V ...]

The route (tools/screen_diff/routes/*.route) is a list of tokens, counted in VI frames from power-on, as tools/tour.py:
  A B Z START L R UP DOWN LEFT RIGHT CU CD CL CR, A+B ...   tap (held 6 frames, then 1 idle)
  hold:A:30                                                hold 30 frames (then 1 idle)
  <number>                                                 wait that many frames
  shot:<label>                                             a checkpoint: capture this frame on both sides
  sync:<event>                                             a sync point: each side waits here until its game reaches
                                                           the event, then goes on counting frames (events: cmd = a
                                                           battle command phase starts, move = a move's presentation
                                                           starts, after its "X used Y" message);
                                                           put it right after the press that leads to the event
and header lines:
  @sram <file>         start from this Stadium Zero SRAM (32 KiB, N64 byte order; relative to the route file or $ZERO_SAVES);
                       default: a blank save on both sides
  @gb <name> [<save>]  a Transfer Pak with the Game Boy ROM $GB_ROM_<NAME> and the save $GB_SAV_<NAME> (if set), or
                       with <save>, $GB_SAV_<NAME>_<SAVE> (e.g. `@gb green lab`: $GB_SAV_GREEN_LAB, the Lab routes' save)
  @window <before> <after>  the ares frames kept around each checkpoint (default 12 before, 120 after): the port is
                       usually ahead of ares after a load (it loads faster, e.g. the P-OS "NOW LOADING" screen), so
                       tools/screen_diff/compare.py compares the port picture with each of these and keeps the closest;
                       the offset it picked is reported (timing, not rendering)
  @boot_offset <n>     port frame = ares frame + n for checkpoints before the first press (default -165: the port
                       skips the N64's boot and runs the intro a few percent fast, so it is that far ahead of ares
                       until the first input; presses happen on the same frame on both sides, and the game waits for
                       them, which lines the two up again)
  @rng <seed>          seed the battle's random numbers alike on both sides (PORT_XCHECK_RNG / ARES_REF_XCHECK_RNG:
                       Battle_Random, the COM's BattleAi_StirRandom and guRandom); without it battles differ, since
                       both games time their random numbers with the CPU counter. The values drawn go to
                       <out>/port_rng.txt and <out>/ares_rng.txt (run.py compares them). Both sides also log each
                       battle scene state change to <out>/port_phase.txt / ares_phase.txt, and run.py lists the
                       states that last a different number of frames (phase_diff.py: where the pacing differs)
  @weak_com            one hit wins: whenever 1P reads its command buttons, the COM's shown Pokémon is left at 1 HP
                       and the rest of its team at 0, alike on both sides (PORT_DEBUG_WEAK_COM / ARES_REF_WEAK_COM),
                       to reach later tournament rounds and the Cup clear; use it with @rng and sync points
# starts a comment.

The port runs hidden at 640x480 (PORT_RESOLUTION=2x, 4:3; SCREEN_DIFF_PORT_RESOLUTION overrides it: Zero draws its
menus and text in 640x480, and at native 320x240 every other texel row and column is dropped, which makes the small
font look wrong) in the original text (PORT_LANGUAGE=kana: no language select screen), lockstep at full speed, from a recording with every current behaviour switch on. ares runs
tools/ares_ref (build it with tools/ares_ref/build.sh). Pictures go to <out>/port/<label>.ppm and
<out>/ares/<label>/f_<frame>.png; nothing goes into the repository (the pictures are the game's own: keep them out of
git). Environment: ROM (the Pocket Monsters Stadium .z64), PORT_BIN (default build/pokestadium), ARES_REF_BUILD and
ARES_REF_SRC (default build/ares/{build,ares}).
"""
import os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
BUTTONS = {"A": 0x8000, "B": 0x4000, "Z": 0x2000, "START": 0x1000, "UP": 0x0800, "DOWN": 0x0400, "LEFT": 0x0200,
           "RIGHT": 0x0100, "L": 0x0020, "R": 0x0010, "CU": 0x0008, "CD": 0x0004, "CL": 0x0002, "CR": 0x0001}


def parse_route(path):
    """-> dict(presses=[(frame, spec, hold)], shots=[(frame, label)], end, sram, gb, window)"""
    r = dict(presses=[], shots=[], sram=None, gb=None, gbsav=None, window=(12, 120), boot_offset=-165, rng=None, syncs=[], weak_com=False)
    frame = 0
    for raw in open(path, encoding="utf-8"):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if line.startswith("@"):
            key, _, val = line[1:].partition(" ")
            val = val.strip()
            if key == "sram":
                cand = [os.path.join(os.path.dirname(os.path.abspath(path)), val)]
                if os.environ.get("ZERO_SAVES"):
                    cand.insert(0, os.path.join(os.environ["ZERO_SAVES"], val))
                r["sram"] = next((c for c in cand if os.path.exists(c)), cand[-1])
            elif key == "gb":
                r["gb"], _, sv = val.upper().partition(" ")
                r["gbsav"] = "GB_SAV_" + r["gb"] + ("_" + sv.strip() if sv.strip() else "")
            elif key == "window":
                r["window"] = tuple(int(v) for v in val.split())
            elif key == "boot_offset":
                r["boot_offset"] = int(val)
            elif key == "rng":
                r["rng"] = str(int(val, 0))
            elif key == "weak_com":
                r["weak_com"] = True
            else:
                sys.exit("%s: unknown header @%s" % (path, key))
            continue
        for tok in line.split():
            if tok.isdigit():
                frame += int(tok)
            elif tok.lower().startswith("shot:"):
                label = tok.split(":", 1)[1]
                if any(l == label for _, l in r["shots"]):
                    sys.exit("%s: checkpoint %s twice" % (path, label))
                r["shots"].append((frame, label))
            elif tok.lower().startswith("sync:"):
                event = tok.split(":", 1)[1]
                if event not in SYNC_EVENTS:
                    sys.exit("%s: unknown sync event %r (known: %s)" % (path, event, " ".join(SYNC_EVENTS)))
                r["syncs"].append((frame, event))
            elif tok.lower().startswith("hold:"):
                _, spec, n = tok.split(":")
                mask(spec)
                r["presses"].append((frame, spec.upper(), int(n)))
                frame += int(n) + 1
            else:
                mask(tok)
                r["presses"].append((frame, tok.upper(), 6))
                frame += 7
    r["end"] = frame + r["window"][1] + 2
    first = r["presses"][0][0] if r["presses"] else None
    r["port_shots"] = [(f + r["boot_offset"] if first is None or f <= first else f, l) for f, l in r["shots"]]
    return r


SYNC_EVENTS = ("cmd", "move")  # PORT_SYNC events: game/src/fragments/battle (Port_SyncEvent), tools/ares_ref (hooks)


def mask(spec):
    m = 0
    for n in spec.upper().split("+"):
        if n not in BUTTONS:
            sys.exit("unknown button %r in %r (known: %s)" % (n, spec, " ".join(BUTTONS)))
        m |= BUTTONS[n]
    return m


def clean_dir(d):
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)


def run_port(r, out, extra_env):
    rec = os.path.join(out, "port.rec")
    lines = ["# pokestadium input recording v1: frame, then button stick_x stick_y for pads 1-4\n",
             "# game audio\n", "# load waits\n", "# paced logo hold\n", "# tpak timing\n"]
    for f, spec, hold in r["presses"]:
        lines.append("%d %d 0 0 0 0 0 0 0 0 0 0 0\n" % (f, mask(spec)))
        lines.append("%d 0 0 0 0 0 0 0 0 0 0 0 0\n" % (f + hold))
    lines.append("# end %d\n" % r["end"])
    open(rec, "w").writelines(lines)
    shots = os.path.join(out, "port_raw")
    clean_dir(shots)
    final = os.path.join(out, "port")
    clean_dir(final)
    sram = os.path.join(out, "port.sra")
    if r["sram"]:
        shutil.copyfile(r["sram"], sram)
    elif os.path.exists(sram):
        os.remove(sram)
    home = os.path.join(out, "port_home")  # a fresh settings directory: nothing from earlier runs or the launcher
    clean_dir(home)
    env = dict(os.environ, PORT_REPLAY=rec, PORT_STOP_AT=str(r["end"]), PORT_SCREENSHOT_DIR=shots,
               PORT_SHOT_AT=",".join(str(f) for f, _ in r["port_shots"]), PORT_SPEED="max", PORT_HIDDEN_WINDOW="1",
               PORT_LANGUAGE="kana", PORT_ASPECT="4:3", PORT_UPSCALE="exact",
               PORT_RESOLUTION=os.environ.get("SCREEN_DIFF_PORT_RESOLUTION", "2x"),
               PORT_SRAM_FILE=sram, PORT_RECORD=os.path.join(out, "port_last.rec"), SHIP_HOME=home)
    env.setdefault("SDL_AUDIO_DRIVER", "dummy")
    if r["gb"]:
        env["PORT_GB_ROM"] = os.environ["GB_ROM_" + r["gb"]]
        if r["gbsav"] in os.environ or r["gbsav"] != "GB_SAV_" + r["gb"]:
            gbsav = os.path.join(out, "port_gb.sav")
            shutil.copyfile(os.environ[r["gbsav"]], gbsav)
            env["PORT_GB_SAVE"] = gbsav
    if r["syncs"]:
        env["PORT_SYNC"] = ",".join("%d:%s" % s for s in r["syncs"])
        env["PORT_SYNC_LOG"] = os.path.join(out, "port_sync.txt")
    if r["rng"]:
        env["PORT_XCHECK_RNG"] = r["rng"]
        env["PORT_XCHECK"] = os.path.join(out, "port_rng.txt")
        env["PORT_PHASELOG"] = os.path.join(out, "port_phase.txt")
    if r["weak_com"]:
        env["PORT_DEBUG_WEAK_COM"] = "1"
    env.update(extra_env)
    binary = os.path.abspath(os.environ.get("PORT_BIN", os.path.join(ROOT, "build", "pokestadium")))
    with open(os.path.join(out, "port.log"), "w") as log:
        subprocess.run([binary, os.environ["ROM"]], cwd=os.path.dirname(binary), env=env, stdout=log,
                       stderr=subprocess.STDOUT, timeout=int(os.environ.get("SCREEN_DIFF_TIMEOUT", "3600")))
    got = sorted(int(f[5:-4]) for f in os.listdir(shots) if f.startswith("stop_") and f.endswith(".ppm"))
    missing = []
    for f, label in r["port_shots"]:
        cand = [g for g in got if g >= f]
        if cand:
            shutil.copyfile(os.path.join(shots, "stop_%05d.ppm" % cand[0]), os.path.join(final, label + ".ppm"))
            open(os.path.join(final, label + ".frame"), "w").write("%d\n" % cand[0])
        else:
            missing.append(label)
    shutil.rmtree(shots)
    for l in open(os.path.join(out, "port.log"), errors="replace"):
        if l.startswith("*** crash"):  # ([freeze] lines only say a screen stood still, e.g. a menu waiting)
            print("port:", l.strip())
            break
    if missing:
        print("port: no picture for", " ".join(missing))


def run_ares(r, out):
    build = os.environ.get("ARES_REF_BUILD", os.path.join(ROOT, "build", "ares", "build"))
    src = os.environ.get("ARES_REF_SRC", os.path.join(ROOT, "build", "ares", "ares"))
    adir = os.path.join(out, "ares")
    clean_dir(adir)
    w = r["window"]
    lines = []
    # ares frame n is the port's frame n-1 (the port's replay latches frame f's pad for the frame drawn after it)
    for f, spec, hold in r["presses"]:
        lines.append("hold %d %d %s\n" % (f + 1, f + hold, spec))
    for f, label in r["shots"]:
        d = os.path.join(adir, label)
        os.makedirs(d)
        lines.append("window %d %d %s\n" % (max(1, f + 1 - w[0]), f + 1 + w[1], d))
    for f, event in r["syncs"]:
        lines.append("sync %d %s\n" % (f + 1, event))
    lines.append("end %d\n" % (r["end"] + 1))
    scr = os.path.join(out, "ares.txt")
    open(scr, "w").writelines(lines)
    env = dict(os.environ, ARES_REF_FIRMWARE=os.path.join(src, "mia", "Firmware", "Nintendo 64"))
    if sys.platform == "darwin":
        env["DYLD_LIBRARY_PATH"] = "/Applications/ares.app/Contents/Frameworks"
    if r["sram"]:
        env["ARES_REF_SRAM"] = os.path.abspath(r["sram"])
    if r["gb"]:
        env["ARES_REF_GBROM"] = os.environ["GB_ROM_" + r["gb"]]
        if r["gbsav"] in os.environ or r["gbsav"] != "GB_SAV_" + r["gb"]:
            env["ARES_REF_GBSAV"] = os.environ[r["gbsav"]]
        env.setdefault("ARES_REF_GBBOARD", "MBC3" if r["gb"] in ("YELLOW", "PIKACHU") else "MBC1")
    if r["syncs"]:
        env["ARES_REF_SYNC_LOG"] = os.path.join(out, "ares_sync.txt")
    if r["rng"]:
        env["ARES_REF_XCHECK_RNG"] = r["rng"]
        env["ARES_REF_XCHECK"] = os.path.join(out, "ares_rng.txt")
        env["ARES_REF_PHASELOG"] = os.path.join(out, "ares_phase.txt")
    if r["weak_com"]:
        env["ARES_REF_WEAK_COM"] = "1"
    link = os.path.join(out, "rom.z64")  # nall rewrites path arguments with spaces
    if os.path.lexists(link):
        os.remove(link)
    os.symlink(os.path.abspath(os.environ["ROM"]), link)
    exe = os.path.join(build, "ares_ref.exe" if os.name == "nt" else "ares_ref")
    with open(os.path.join(out, "ares.log"), "w") as log:
        subprocess.run([exe, link, scr], env=env, stdout=log, stderr=subprocess.STDOUT,
                       timeout=int(os.environ.get("SCREEN_DIFF_TIMEOUT", "3600")))
    os.remove(link)
    # ~130 frames per checkpoint at 460 KB each as PPM: keep them as PNG (about a fifth of that)
    try:
        from PIL import Image
        for _, l in r["shots"]:
            d = os.path.join(adir, l)
            for f in os.listdir(d):
                if f.endswith(".ppm"):
                    Image.open(os.path.join(d, f)).save(os.path.join(d, f[:-4] + ".png"), compress_level=1)
                    os.remove(os.path.join(d, f))
    except ImportError:
        pass
    empty = [l for _, l in r["shots"] if not os.listdir(os.path.join(adir, l))]
    if empty:
        print("ares: no picture for", " ".join(empty))


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        sys.exit(__doc__)
    route, out = args[0], os.path.abspath(args[1])
    flags = args[2:]
    if "ROM" not in os.environ:
        sys.exit("set ROM to the Pocket Monsters Stadium (Japan) .z64")
    r = parse_route(route)
    if "--window" in flags:
        r["window"] = tuple(int(v) for v in flags[flags.index("--window") + 1].split(","))
        r["end"] = max([f for f, _ in r["shots"]] + [f + h for f, _, h in r["presses"]]) + r["window"][1] + 2
    extra = {}
    for i, f in enumerate(flags):
        if f == "--port-env":
            k, v = flags[i + 1].split("=", 1)
            extra[k] = v
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "route.txt"), "w") as f:
        f.write("route %s\nwindow %d %d\nboot_offset %d\n" % ((os.path.abspath(route),) + r["window"] + (r["boot_offset"],)))
        for fr, label in r["shots"]:
            f.write("shot %d %s\n" % (fr, label))
    if "--ares-only" not in flags:
        run_port(r, out, extra)
    if "--port-only" not in flags:
        run_ares(r, out)
    if r["syncs"]:
        check_sync(out, len(r["syncs"]))
    if r["rng"]:
        check_rng(out)
        if os.path.exists(os.path.join(out, "port_phase.txt")) and os.path.exists(os.path.join(out, "ares_phase.txt")):
            import phase_diff
            phase_diff.report(os.path.join(out, "port_phase.txt"), os.path.join(out, "ares_phase.txt"))
    print("%s: %d checkpoints, %d frames -> %s" % (os.path.basename(route), len(r["shots"]), r["end"], out))


def check_sync(out, n):
    """How long each side waited at each sync point (a timeout: the event never came, so the two went apart)."""
    for side in ("port", "ares"):
        path = os.path.join(out, side + "_sync.txt")
        rows = [l.split() for l in open(path)] if os.path.exists(path) else []
        waits = " ".join("%s:%s%s" % (r[2], r[5], "(timeout)" if "timeout" in r else "") for r in rows)
        print("%s sync points reached %d/%d, waited %s" % (side, len(rows), n, waits or "-"))


def check_rng(out):
    """Compares the random values both games drew (@rng): the same function, call and value in the same order, or
    the battles went apart (a function called a different number of times, e.g. one side's extra turn)."""
    def draws(name):
        path = os.path.join(out, name)
        if not os.path.exists(path):
            return None
        return [tuple(l.split()[1:5]) for l in open(path) if l.startswith("R ")]
    port, ares = draws("port_rng.txt"), draws("ares_rng.txt")
    if port is None or ares is None:
        return
    first = next((i for i, (a, b) in enumerate(zip(port, ares)) if a[0::3] != b[0::3] or a[1] != b[1]), None)
    if first is None and len(port) == len(ares):
        print("random numbers: the same %d draws on both sides" % len(port))
    elif first is None:
        print("random numbers: the first %d draws agree; port drew %d, ares %d" % (min(len(port), len(ares)), len(port), len(ares)))
    else:
        print("random numbers: draw %d differs (port %s, ares %s)" % (first, " ".join(port[first]), " ".join(ares[first])))


if __name__ == "__main__":
    main()
