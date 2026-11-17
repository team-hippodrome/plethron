# Lab PC boxes read through an `s32*` view of a pointer list / N64 row stride (Check BOX crash, icons shifted)

- **Category:** abi-64bit
- **Zero status:** needed (Zero's `LabBoxList_ApplyChecks` walks the box rows with the N64 stride and offset)
- **stadion:** [PR #70](https://github.com/team-hippodrome/stadion/pull/70), commit(s) `567532e`
- **stadion files:** `patches/game/20-fragment-lab_pc.patch` (`func_882052F4`, `func_88201488`)

## Symptom
Lab PC -> Organize -> BOXes -> "Check BOX" -> A crashed (SIGSEGV in `LabPC_CheckBox_ShowBox`); every box icon was shifted.

## Root cause
`func_882052F4` took the chosen box from the grid's list of box pointers through an `s32*` cast, so it got only the pointer's low 4 bytes. The cell draw loaded the icon texture from `arg2 + 2` on a pointer-to-pointer: 8 bytes in on the N64, 16 on the host.

## Fix in stadion
Index the typed pointer list; read the icon through the real struct field.

## Zero / plethron
Zero's Lab PC box screens index typed lists (`game/src/fragments/lab_pc/pc_box.c:925-940`, `*(LabPCBoxEntry**)menu->items`). But the same class is live in いちらん's box lists:
`game/src/fragments/lab_pokemon_list/popup_menus.c:358-365` `LabBoxList_ApplyChecks`:
```c
s32* entry = (s32*)box_list->menu.items->rows;
for (i = 0; i < box_list->menu.items->count; i++) {
    entry[3] = (box_list->menu.itemFlags[i] & 8) != 0;  // N64 offset 0x0C
    entry += 8;                                          // N64 row size 0x20
}
```
The rows are `LabPCBox` (`lab_pokemon_list.h:181-188`: `Entry89100140* source` first, so on a 64-bit host `checked` is at 0x10 and the row is 0x28 bytes), initialised as `LabPCBox` in `pokemon_list_10.c:44-66` and read typed by `LabBoxList_UpdateItemFlags` (`popup_menus.c:325-334`). So on OK (`popup_menus.c:705-706`) the box checks go to `unk_08` of box 0 and into the middle of later rows, not to `checked`. Fix: iterate `LabPCBox*` and set `box->checked`.

## How to detect
`grep -rn -E '\((s32|u32|int)\s*\*\)' game/src/fragments` on rows/items of tables that hold pointers; `entry += N` with a constant row size next to a struct containing pointers.
