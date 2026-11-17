# Gallery prompts drawn as empty boxes: string getters fell off the end without returning

- **Category:** game-logic
- **Zero status:** not applicable (no Gallery in Zero; the bug class is audited in `074-...`)
- **stadion:** [PR #72](https://github.com/team-hippodrome/stadion/pull/72), commit(s) `c9b9b18`
- **stadion files:** `patches/game/20-fragment-gallery_backgrounds.patch`

## Symptom
Snap photo Pokémon-select prompts for an unreadable Game Pak ("Save file not found.") were empty boxes.

## Root cause
`Gallery_GetCommonUiString` and `Gallery_CopyCommonUiString` called `Text_GetString` without returning its result. On the N64 the value stayed in `v0` by accident; on the host the caller got garbage.

## Fix in stadion
`return Text_GetString(...);` in both.

## Zero / plethron
Zero has no Gallery (pokestadium0 `docs/layout.md`). For the same class in Zero's code (34 non-void functions that fall off the end), see `074-functions-falling-off-the-end-v0.md`.

## How to detect
`clang -fsyntax-only -Wno-everything -Wreturn-type -Wreturn-mismatch` over game/src; plethron builds the game with `-Wno-return-type` (`CMakeLists.txt` STADION_GAME_WARNINGS), so the build never shows them.
