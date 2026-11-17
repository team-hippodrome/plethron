# Screen diff: the port against ares

Plays the same input on the port and on [ares](https://ares-emu.net) (the reference: a cycle-accurate N64 with
paraLLEl-RDP), saves the picture at each checkpoint on both sides and ranks the checkpoints by how much the port's
picture differs from ares's. Same approach as stadion's `tools/tour.py`, made to cope with Zero's timing (below).

## Setup (once)

```bash
# ares with the headless reference driver tools/ares_ref (N64 + Game Boy cores). Linux without a GPU: Mesa's
# software Vulkan (sudo apt install mesa-vulkan-drivers libvulkan-dev) is enough, ~50 frames/s.
tools/ares_ref/build.sh build/ares            # safe to rerun
pip install numpy pillow
# a Japanese Green cartridge save with a party, for the routes that need the Transfer Pak
python3 tools/screen_diff/jp_gen1_save.py saves/green_party.sav
# the GB Player routes: the same save for Red, Blue and Pikachu, and a Zero save with the GB Player's frames and speed
# unlocked (from any save the game wrote, e.g. <out>/<route>/port.sra of a route without @sram)
for g in red blue pikachu; do python3 tools/screen_diff/jp_gen1_save.py saves/${g}_party.sav; done
python3 tools/screen_diff/zero_sram_options.py <a Zero save> saves/gb_unlocked.sra --gb-frames 2
# the Pokédex route's save: owned, held, seen-only and unseen species
python3 tools/screen_diff/jp_gen1_save.py saves/green_dex.sav --owned 1-8,12-130 --seen 9-10,131-150
# the Lab routes' save: six in the party, eight in Game Boy box 1, eight items in the bag
python3 tools/screen_diff/jp_gen1_save.py saves/green_lab.sav --lab
```

Environment for the runs (the ROMs are yours and stay out of the repository):

| variable | |
|---|---|
| `ROM` | Pocket Monsters Stadium (Japan) `.z64` |
| `ARES_REF_BUILD`, `ARES_REF_SRC` | default `build/ares/build`, `build/ares/ares` (what `build.sh build/ares` makes) |
| `GB_ROM_GREEN`, `GB_SAV_GREEN` | Pocket Monsters Green `.gb` and the save from `jp_gen1_save.py` |
| `GB_ROM_RED`, `GB_ROM_BLUE`, `GB_ROM_PIKACHU` (+ `GB_SAV_*`) | the other Japanese carts for the `gb_*` routes; `GB_ROM_GREEN_EMPTY` = the Green `.gb` again, with no save (`gb_cart_check`) |
| `ZERO_SAVES` | directory with `gb_unlocked.sra` (`gb_frames_speed`) |
| `GB_ROM_GREENDEX`, `GB_SAV_GREENDEX` | the same Green `.gb` and `saves/green_dex.sav` (route `pokedex`) |
| `GB_SAV_GREEN_LAB` | `saves/green_lab.sav` (routes `party`, `list`, `organize`, `register`: `@gb green lab`) |
| `PORT_BIN` | default `build/pokestadium` |

On Linux without a display, start `Xvfb :99` and set `DISPLAY=:99` (the port's window is hidden but still needs one).

## Running

```bash
python3 tools/screen_diff/run_all.py out/screens          # every route; ~6 minutes on 4 cores
python3 tools/screen_diff/run_all.py out/screens tools/screen_diff/routes/pokedex.route   # one route
python3 tools/screen_diff/run.py <route> <dir> --port-only  # after a port fix: rerun the port, keep ares's pictures
python3 tools/screen_diff/compare.py out/screens/report out/screens/*/
```

`out/screens/report/report.md` lists the checkpoints worst first: score (mean absolute RGB difference at 320x240
after a slight blur, 0 = identical), how many frames apart the closest ares frame was, colour bias, detail ratio and
short hints. `report/cmp/<route>__<checkpoint>.png` shows port | ares | difference (red where they differ).
The pictures are the game's own, so they never go into git.

## Routes

`routes/*.route`: button presses and waits counted in VI frames from power-on, and `shot:<label>` checkpoints (format
in `run.py`). To write one, find the moments in ares first (`tools/ares_ref/ref.sh` with `every <n> <dir>`), then
put checkpoints where the screen has settled or right after the press that starts an animation.

| route | covers |
|---|---|
| `intro_attract` | N64 logo, intro Pokémon, title, the attract-mode battle (no cartridge) |
| `free_battle` | lobby, cartridge select, P-OS, main menu, Battle, Free Battle vs COM, entry, arena intro, VS, order screen |
| `tournament` | Battle → tournament menus |
| `tournament_l30` | L1～30 tournament, Poké Ball Cup round 1: cup select, rentals (5 + the cart's Pikachu), VS scene (round title, trainer panels, teams), order screen, battle intro, surrender, results board, かち / まけ, back to the menu (`@rng`) |
| `tournament_l50` | the same for the L50～55 tournament (no cup select, all six from rentals) |
| `pokedex` | ずかん list and an entry |
| `gb_player` | GB Player booting the Green cartridge |
| `gb_red`, `gb_green`, `gb_blue`, `gb_pikachu` | each Japanese cart in the GB Player: boot, border, intro, title, new game, Prof. Oak's intro (A every 120 frames); Pikachu first shows the cartridge copy screen |
| `gb_menu` | the C-Up menu and its three help texts, the START/SELECT config screen and choices, then quit to Stadium |
| `gb_continue_error` | continue with the crafted save: Zero's レポートがただしくないので screen |
| `gb_frames_speed` | frames and speed unlocked (`zero_sram_options.py --gb-frames 2`): Z cycles Dodrio, green, Doduo, Dodrio; C-Right x2, x4, x1; a new game at x4 |
| `gb_cart_check` | Green with an empty save: ????? on the cartridge select, GBのポケモンがいない main menu, GB Player |
| `party`, `list`, `organize`, `register` | てもち, いちらん, せいとん, とうろく |
| `rules_free_battle`, `rules_l30`, `rules_l50` | ルールを見る of Free Battle and both tournaments: the rules list, every rule page and the usable Pokémon grid (port checked; checkpoints not yet tuned against ares) |
| `battle` | Free Battle vs COM to a two-turn battle (Pikachu vs Pikachu), random numbers seeded (`@rng`) |
| `main_menu` | the main menu with a party cartridge: the cursor on every item, the バトル prompt, OFF > はい back to the cartridge check screen and in again |
| `no_cart` | no Transfer Pak: the cartridge check screen, the main menu with the Game Boy items greyed out, the バトル and OFF prompts |
| `options` | the cartridge check screen's Options (C-Right): every row, Sound/Commentary toggles saved and discarded, both Backup questions and a save clear, B back to the title |
| `pokedex` | ずかん list (number and kana order, scrolled to the end), データをみる pages with the turning 3D model (held and seen-only species), ぶんぷをみる habitat maps (unknown habitat; ポッポ with the GB map, zoom and the next area), やめる |
| `party` | てもち: list, つよさをみる (stats page, moves page and each move's description), ならびかえ, レポートしてやめる |
| `list` | いちらん: source dialog with the Game Boy box, the table's three pages, L/R mode, a row's menu, どうぐをつかう (Protein's stat pop-up), あつめる |
| `organize` | せいとん: ポケモンをいどうする (party to 64 box 1, report), どうぐをいどうする (lists, count picker), ボックスをいどうする (GB box 1 to 64 box 2, report) |
| `register` | とうろく: register the party (the Ditto transform), view it, the delete screen |

## Timing: why there is a window

The port and ares do not run frame for frame alike: the port skips the N64 boot (~165 frames ahead at the first
picture), runs the intro a few percent faster, and loads faster (the P-OS "NOW LOADING" screen is shorter). Presses
land on the same frame on both sides, and menus wait for them, which lines the two up again. So the port takes one
picture per checkpoint (`@boot_offset` shifts the ones before the first press) and ares keeps every frame from 12
before to 120 after it (`@window`); `compare.py` scores the port picture against the closest of those. The chosen
offset is in the report: a large one is a timing difference, not a rendering one.

## Battles: seeded random numbers (`@rng <seed>`)

Both games time their random numbers with the CPU counter, so without help the same inputs give different battles.
`@rng <seed>` makes the battle's two osGetCount reads (`Battle_Random`, the COM's `BattleAi_StirRandom`) return a
seeded stream instead, and restarts `guRandom` (particles) at each battle's start and each move, on both
sides: `PORT_XCHECK_RNG` in the port (`src/port/xcheck.cpp`) and `ARES_REF_XCHECK_RNG` in ares_ref (CPU host hooks:
`build.sh` applies `ares-host-hooks.patch`). The values drawn go to `port_rng.txt` and `ares_rng.txt` in the run
directory, and run.py says whether they agree.

The order screen takes 1P's picks only after the COM panel has made its own (about 250 frames after the screen
appears); presses before that are dropped in both games.

Known limits: the attract battle (no presses) is not seeded the same way yet: its Pokémon come from the title
screen's own generator. In battle the N64 slows down in busy effects while the port does not, so ares falls further
behind during a battle and fixed-frame presses late in a battle can land in a different state.
