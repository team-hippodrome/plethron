# Registering a team froze: a 4-entry ring declared as [3] plus N64-sized padding

- **Category:** abi-64bit
- **Zero status:** not applicable (Zero's registration has no 4-entry linked ring)
- **stadion:** [PR #71](https://github.com/team-hippodrome/stadion/pull/71), commit(s) `4a6f4b8`
- **stadion files:** `patches/game/20-fragment-rental_team_selection.patch` (`unk_D_84229EB0`)

## Symptom
Gym Leader Castle -> Registration -> Register Pokémon (or any registration menu) froze on the "Register Pokémon" screen once the team was saved.

## Root cause
The registration manager keeps a ring of four team entries linked `[(i +/- 1) & 3]`. The struct declared `unk_00028[3]` followed by `char unk0E7B8[0x4D30]`, the N64 size of the fourth entry. The host entry is larger (two pointers), so the fourth entry's back link landed on the team tray pointer; the tray resume saw a wrong state and did nothing, and the input loop never ended.

## Fix in stadion
Declare `unk_00028[4]`.

## Zero / plethron
Zero's registration fragment is a separate, simpler design (`game/src/fragments/registration/registration.c`, `registration.h`, 51 functions); grep finds no `(i +/- 1) & 3` links in any fragment (only `model_callbacks.c:42`, a 4-slot buffer index) and no large `char` pad standing in for array elements in `registration.h`.

## How to detect
grep for `[N]` arrays immediately followed by `char unkXXXX[size]` where size equals one element's N64 size; any pad whose size matches a struct that contains pointers. `tools/layout_check.py` in plethron (struct view pairs that agree on N64 offsets but not on the host).
