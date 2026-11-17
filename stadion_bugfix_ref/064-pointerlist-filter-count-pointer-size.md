# Lab list filters: PointerList_Filter counted 4-byte pointers (count doubled, garbage rows, crash)

- **Category:** abi-64bit
- **Zero status:** already ported (plethron 5134413: `arg1->count = var_s1 - arg1->items`)
- **stadion:** [PR #64](https://github.com/team-hippodrome/stadion/pull/64), commit(s) `971fe15`
- **stadion files:** `patches/game/02-core.patch` (`game/src/table_view.c`)

## Symptom
Lab PC See List: picking a filter from a row's menu (e.g. "Group BULBASAUR") segfaulted in `func_88304850` on a garbage Pokémon pointer.

## Root cause
`PointerList_Filter` set the filtered list's count to the byte difference of two pointer cursors `>> 2`, i.e. assuming 4-byte N64 pointers. On 64-bit hosts the count is doubled and the grid draws rows past the filled entries.

## Fix in stadion
```c
arg1->unk_04 = var_s1 - arg1->unk_00; // count entries, not bytes >> 2
```

## Zero / plethron
Unfixed: `game/src/table_view.c:398` `arg1->count = ((uintptr_t)var_s1 - (uintptr_t)arg1->items) >> 2;` where `var_s1` is `LabMonEntry**`. Zero calls it through the alias `func_80027D94` (`src/port/generated/zero_aliases.ld:121`) for every いちらん filter: `game/src/fragments/lab_pokemon_list/pokemon_list_4.c:224-244` (`LabPCList_ApplyFiltersAndSort`: level range, type, move, species, type pair, battle-eligible). Fix: `arg1->count = var_s1 - arg1->items;`.

## How to detect
`grep -rn -E '\(uintptr_t\).*-.*\(uintptr_t\).*>> ?[234]' game/src` and `>> 2` on pointer differences; ASan on list filter routes.
