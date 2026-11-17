# Calls that did not match their functions: stub signatures and a K&R call with an extra argument

- **Category:** abi-64bit
- **Zero status:** already ported (inherited `src/port/stubs.c`); the minigame call is Stadium-1-only
- **stadion:** [PR #52](https://github.com/team-hippodrome/stadion/pull/52), commit(s) `f0a0b1f`
- **stadion files:** `src/port/stubs.c`, `patches/game/20-fragment-minigame_select.patch`

## Symptom
No visible failure; undefined behaviour that depends on the calling convention (return values read from registers the callee never set, arguments where the callee does not look). Matters for WebAssembly (traps on signature mismatch) and some ABIs.

## Root cause
Stubs declared differently from the game: `_doModFunc` (game: `f32 _doModFunc(ALDelay*, s32)`, n_reverb modulation) took two pointers and returned a pointer, so the float return was read from a register never set; `alSynNew` returned `s32` instead of `void`; `__LeoBootGame3` took no argument. The minigame-select fragment called `Ui_DrawTextureMarker` with three arguments through a K&R `extern void Ui_DrawTextureMarker();`.

## Fix in stadion
Stubs get the game's signatures (`_doModFunc` returns `0.0f`); the minigame call passes two arguments.

## Zero / plethron
Stubs present: `src/port/stubs.c:105` (`void __LeoBootGame3(void* entry)`), `:115` (`void alSynNew(...)`), `:117` (`float _doModFunc(void* d, s32 count) ... return 0.0f;`), matching Zero's `game/src/libnaudio/n_reverb.c:10` (`extern f32 _doModFunc(ALDelay* d, s32 count);`). Minigame select does not exist in Zero. Zero still has ~175 K&R empty-parameter declarations in `game/src` (e.g. `game/src/menu.c:124-125`, `game/src/pokemon_stats.h:112`), each a candidate for the same mismatch.

## How to detect
`clang -Wdeprecated-non-prototype -Wstrict-prototypes` over game/src; `tools/abi_audit/abi_calls.py` in plethron (calls through `func_X` aliases declared with other parameter types); a WebAssembly build traps on every mismatched indirect/direct call.
