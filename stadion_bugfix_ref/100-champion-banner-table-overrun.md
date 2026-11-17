# Castle Champion transition crash: location 9 indexed one past a nine-entry texture table

- **Category:** game-logic
- **Zero status:** not applicable (Gym Leader Castle / Elite Four / Champion are Stadium 1 only)
- **stadion:** [PR #100](https://github.com/team-hippodrome/stadion/pull/100), commit(s) `e640185` (merge `2db362c`)
- **stadion files:** `patches/game/20-fragment-battle_prep.patch`, whole-game catalog probe and tests

## Symptom
Access violation during the Elite Four -> rival/Champion transition (frame 109636 of a 150,000-frame route, all
renderers).

## Root cause
Castle location 9 indexed `D_84B0FC6C[9]`, a table with nine entries. On N64 the next table's first entry happened
to be the Champion banner texture (`D_3048210`); the host linker put unrelated bytes there, giving an invalid
segmented texture pointer.

## Fix in stadion
Add the explicit tenth entry `D_3048210` to the table (no renderer guard, no error suppression).

## Zero / plethron
Zero has no Gym Leader Castle (`ls game/src/fragments` shows no castle/battle_prep badge carousel). The pattern
still matters for Zero: any decomp table read one past its end that relied on the next ROM symbol.

## How to detect
Out-of-range indices into game tables: compare loop/index bounds with `sizeof` of the lifted array; ASan or a guard
page after lifted data (`src/port/generated/zero_lift_data.c` sizes) catch reads past the end. A texture pointer
that is neither segmented nor host memory in the renderer log is a symptom.
