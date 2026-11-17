# Move coverage: every battle Pokémon using every move, port against ares

Stadium Zero has 40 Pokémon that can battle, and any number of move animations of its own. Rather than guess which
ones matter, this plays all of them: each of the 40 species uses each move Zero lets it have, on the port and on
[ares](https://ares-emu.net), and `tools/screen_diff` compares the pictures.

```bash
# once: the port, ares_ref and Xvfb as in tools/screen_diff/README.md (no Game Boy cartridge or save is needed)
export ROM=<Pocket Monsters Stadium (Japan) .z64> DISPLAY=:99
python3 tools/move_coverage/run.py all out/moves --jobs 3     # everything: two passes, then out/moves/coverage.md
# or step by step
python3 tools/move_coverage/run.py plan out/moves               # moveset dump, teams, saves, menu routes
python3 tools/move_coverage/run.py play out/moves --battles 0-3 # the port plays battles 0-3, writes their routes
python3 tools/move_coverage/run.py diff out/moves --battles 0-3 # those routes on the port and on ares, compared
python3 tools/move_coverage/run.py report out/moves             # out/moves/coverage.md
```

## What it covers

The move list is Zero's own: `PORT_DUMP_MOVESETS=<file>` makes the port run `Battle_CheckMovesetLegality` (start
moves, the Red/Green and Yellow level-up tables, TM/HM, and the pre-evolutions' moves) for every species of the
Free Battle table and every move, from your ROM, and exit. That is 1,117 species-move pairs (140 different moves).
Nothing from the ROM is stored in the repository; the dump, the saves and the pictures stay in the work directory.

## How a battle is played

* **Teams.** The pairs are grouped four to a test Pokémon (292 of them), six to a team (49 battles), each team with
  distinct species, at most one Rage user and the status moves spread out. Moves that need another move go together
  with it (a healing move with Substitute or a recoil move to lose HP first; Dream Eater with a sleep move). Level
  50, or the lowest level the moves allow; 100 for one-hit-KO moves (they fail against a faster target).
* **No cartridge.** The teams are written into a crafted Zero save as とうろく registered teams
  (`zero_teams.py`), which Free Battle's entry screen lists: 1P enters a test team, the COM the passive team in
  registration slot 0: Chansey, Snorlax, Vaporeon, Blastoise, Lapras and Cloyster at L100, knowing only moves that
  do nothing to 1P (Reflect, Light Screen, Harden, Withdraw, Amnesia, Acid Armor, Defense Curl, Focus Energy,
  Soft-Boiled), Normal and Water types so that almost nothing of 1P's is immune.
* **The planner** (`planner.py`) runs the port with `PORT_PLANNER_OUT` / `PORT_PLANNER_IN` (src/port/os_devices.cpp):
  at each command phase the port sends both sides' battle state (src/port/move_coverage.c) and waits for the
  script's presses; it also reports every move animation that starts. A test move counts as done when its own
  animation started in the turn it was picked. Moves wait until they can work (a status move needs a target without
  that status, Dream Eater a sleeping one, healing missing HP, Leech Seed a non-Grass target, no Ghost moves on
  Normal types, ...); when the active Pokémon has nothing usable 1P switches, and when nothing at all is usable it
  attacks with a move already done, to knock the COM's Pokémon out (the next one is fresh). A move that does not
  animate in four tries (ten for one-hit-KO moves) is reported as not animated. Rage never ends, so it comes last,
  then 1P surrenders (START, RIGHT, A).
* **The route.** Everything the planner pressed becomes `battle.route`, a `tools/screen_diff` route with seeded
  random numbers (`@rng`) and a sync point at every command phase and every move, plus checkpoints 2, 30, 60, 90 and
  120 frames into each 1P move. `diff` plays it on the port and on ares and runs `compare.py`; ares's frames are
  deleted afterwards (`--keep-frames` keeps them).
* **Second pass.** `all` plans the moves that were not done again, in new teams and battles.

`coverage.md` lists every pair: done (with the worst picture score of its checkpoints and a link to the
port | ares | difference picture), not animated, or not reached.

## Limits

Counter and Bide need damage taken, which the passive COM never deals, so they are expected among the not
animated moves; Teleport, Roar and Whirlwind fail in trainer battles by design. Only 1P's side of each animation is
covered (the COM uses its own few moves): the mirrored camera of a move used by the far side is not.
