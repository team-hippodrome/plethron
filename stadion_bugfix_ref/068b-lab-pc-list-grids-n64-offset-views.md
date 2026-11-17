# Lab PC list: box picker grid, column header page and "Group by TYPE" read through N64-offset views

- **Category:** abi-64bit
- **Zero status:** check (Zero's いちらん screens differ; one equivalent bug found, see 070)
- **stadion:** [PR #68](https://github.com/team-hippodrome/stadion/pull/68), commit(s) `c3a8fca`
- **stadion files:** `patches/game/20-fragment-lab_pc_list.patch` (`lab_pc_list.h`, `BattleInfoWidget_*`, `LabPCList_SwitchColumnPage`, `BattleMoveListWidget_SetMoves`)

## Symptom
Crash in `BattleInfoWidget_DrawEntry` on a NULL cell ("64 BOXes" picker); box cells showed `52/00`; R from the list landed on the wrong column and "Group by TYPE" could not be selected, then crashed.

## Root cause
Several views were declared with N64 offsets: a char-padded struct whose lists line up with the banks only with 0x20-byte banks (0x30 on the host); `char unk00[0x8]` standing for two cells (one pointer on the host); a hard-coded 8-byte stride for `{type, color pointer}` entries (16 on the host); a move id declared as a pointer (16-byte entries the grid reads at 8, writing past the widget).

## Fix in stadion
Bind the grids to the real bank fields (`&bank[1].unk_0C`), make the two cells `WidgetNode*`, use `sizeof` strides, declare the move id `s32`, delete the char-padded views.

## Zero / plethron
Zero's いちらん has its own windows (source select with box lists, type filter, move filter: `game/src/fragments/lab_pokemon_list/lab_pokemon_list.h:280-361`), mostly typed for the host ("PORT: typed" at `lab_pokemon_list.h:180`, `popup_menus.c:303`, `:327`). Still present and worth deleting so nobody uses them: the N64-offset views `unk_func_8830867C_02C_144_000` / `_144` (`lab_pokemon_list.h:16-30`, `char unk08[0x4]` + pointer at 0x0C, size 0x20) and `unk_func_88313894` (`popup_menus.c:12-19`, `pokemon_list_10.c:19-26`); grep finds no `.c` user of them today. One function of this class is still live: `LabBoxList_ApplyChecks` (see `070-lab-pc-box-pointer-list-read-through-s32.md`).

## How to detect
grep headers for `char unkNN[0x4]`/`[0x8]` that sit where a pointer would be, and for structs commented "size = 0x20" that contain pointers; compare `sizeof` on the host with the N64 size comment (`tools/layout_check.py` in plethron).
