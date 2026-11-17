# Non-void game functions that fall off the end: the host returns garbage where the N64 left v0 (VS Mewtwo intro skipped)

- **Category:** game-logic
- **Zero status:** check (taken up by plethron PR #38, which returns what the N64 left in v0 where callers read it, with a check for the class)
- **stadion:** [PR #74](https://github.com/team-hippodrome/stadion/pull/74), commit(s) `7de4f52`
- **stadion files:** `patches/game` (`demo_scenes_169A40.c` `MewtwoIntro_UpdateFade`, Lab PC widget draws, `Pokedex_DrawSortOrderLabel`)

## Symptom
The VS Mewtwo intro was skipped (menu already up at frame 60).

## Root cause
`MewtwoIntro_UpdateFade` falls off the end; on the N64 `v0` still held `ModelAnim_IsAnimationDone()`'s 0 while the animation ran. clang treats the fall-through as unreachable and compiled the function to an unconditional `return 1`. Five widget draw callbacks likewise returned whatever was in `v0` to `WidgetTree_Draw` (nonzero = skip children).

## Fix in stadion
Compiled every game source with `-Wno-everything -Wreturn-type -Wreturn-mismatch -fsyntax-only` (28 hits), checked each caller and the N64 asm for the `v0` value, and added explicit returns where the result is used (`return 0` for the fade, `return 1` for the widgets).

## Zero / plethron
Same scan on plethron's `game/src` (flags from `CMakeLists.txt:325-342`, lus headers) reports 34 functions, e.g. `game/src/ui_graphics.c:92` (`Ui_DrawTexturedRect8x8`), `geo_render.c:646`, `dp_intro.c:89`, `game_state.c:134,168,963`, `save_data.c:382`, `registration/registration.c:1130`, `free_battle_setup/free_battle_setup_5.c:303,861`, `main_menu/main_menu.c:81,334`, `party_summary/mon_summary.c:171`, `lab_pokemon_list/pokemon_list_4.c:249`, `pokemon_list_8.c:674`, 10 in `fragments/battle/`, 7 in `fragments/audio_and_gb_emulator/`. A grep for direct callers that use the result finds only:
- `gb_apu.c:110` `GbApu_GetWaveformSample` (used at `gb_apu.c:237,330,400`): cases 4/5 fall through; callers pass a 2-bit duty or 6/7, as in Stadium 1's `func_81207698` (left alone there).
- `audio_thread.c:175` `AudioThread_ScheduleProcessCmds`, returned by `AudioThread_ResetAudioHeap` (`audio_thread.c:301`), whose caller ignores it (`:314`).
- `audio_load.c:1121` `AudioLoad_SyncInitSeqPlayerInternal`, returned by `AudioLoad_SyncInitSeqPlayer` (`:1055`, no direct caller found).
Not covered by that grep: functions called through pointers (widget/screen/geo callbacks such as `Registration_DrawScreen`, `FreeBattle_DrawScreen`, `LabPCList_DrawScreen`, `ImageGrid_GeoCallback`); check what their dispatchers do with the result.

## How to detect
```sh
clang -fsyntax-only -Wno-everything -Wreturn-type -Wreturn-mismatch <game flags> file.c
```
plethron's build hides these (`-Wno-return-type` in STADION_GAME_WARNINGS, `CMakeLists.txt:277`); `return-type` is enforced only for files in `cmake/strict_sources.txt`.
