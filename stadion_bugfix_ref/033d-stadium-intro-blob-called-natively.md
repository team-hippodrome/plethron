# Castle crash at the stadium intro: the stadium file's MIPS entry was called as native code

- **Category:** abi-64bit
- **Zero status:** not applicable (no Stage_LoadModels / stadium intro in Zero; its one blob-entry call already uses Port_CallBlobEntry)
- **stadion:** [PR #33](https://github.com/team-hippodrome/stadion/pull/33), commit(s) `5c24a88`
- **stadion files:** `patches/game/20-fragment-demo_scenes.patch` (`src/fragments/demo_scenes/demo_scenes_165640.c`)

## Symptom
Bus error at the stadium intro before a cup's last battle or a Gym Leader Castle leader battle.

## Root cause
The stadium file in `stadium_models` is a relocatable fragment whose entry point is a small MIPS function
("return &geoLayout[arg0]"). `Stage_LoadModels` called it through a C function pointer (`sp2C(2, 0)`), jumping into
data. `D_86B0FA38` also held its 32-bit result in a `u32`.

## Fix in stadion
All calls go through the port's MIPS leaf interpreter, as `BattleScene_Init` already did:
```c
D_86B0FA4C = (void*)Port_CallBlobEntry(sp2C, 2, 0);
temp_v0_6 = Port_VResolve(Port_CallBlobEntry(sp2C, 4, 0));
```
`D_86B0FA38` became `uintptr_t` and `Intro_RenderFrame` compares against both -1 forms.

## Zero / plethron
Zero has `Port_CallBlobEntry` (`game/src/memmap.c:208`) and its only caller, the model animation bank lookup, checks
`Port_VIsRaw(fn)` first (`game/src/model_renderer.c:100-101`). Zero has no stadium-intro scene; `BinArchive_GetFile`
is used only in `font.c`, `gb_player_launch.c`, `asset_archive.c`, `display_object_textures.c`,
`free_battle_setup_3/5.c`, `vs_scene.c`, `pokedex_2.c`, none of which call the file as a function.

## How to detect
Crash with PC inside a data buffer (crash report shows a heap address as PC). Grep for calls through a pointer that
came from `BinArchive_GetFile` / a loaded file: `grep -rn "= BinArchive_GetFile" game/src` and inspect the uses.
