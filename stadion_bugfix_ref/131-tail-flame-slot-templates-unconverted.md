# Battle crash with tail-flame Pokemon: static flame draw used slot templates before host conversion

- **Category:** abi-64bit
- **Zero status:** not applicable (Zero's flame templates are typed and converted by the lifter at load, no lazy converter)
- **stadion:** [PR #131](https://github.com/team-hippodrome/stadion/pull/131), commit(s) `4b365b0`
- **stadion files:** `patches/game/02-core.patch` (`src/33FE0.c`, `src/33FE0.h`, `src/model_renderer_callbacks.c`)

## Symptom
SIGSEGV in every battle with Charizard, Charmander, Charmeleon, Magmar or Moltres.

## Root cause
`ModelRenderer_DrawStaticSegment` (flame model, modes 0xFE/0xFF) drew the slot-model templates `D_800777A8` /
`D_80077950` directly. Their tables, Vtx arrays and display lists were raw N64 u32 data that `Port_FixSlotTemplates`
converted to host layout, but that only ran from `func_80033B2C` (first slot build). A static draw before it handed an
8-byte N64 display list to the 16-byte-Gfx host interpreter: G_VTX read the next command as a 64-bit address.

## Fix in stadion
`Port_FixSlotTemplates` made non-static and idempotent, called at the start of the static draw. (Later, PR #142 removed
the converter entirely once the vendored decomp typed the data; see `142b-...`.)

## Zero / plethron
Zero's equivalents are in `game/src/model_renderer_callbacks.c`: `ModelRenderer_UpdateSlotModel` (:145) and
`ModelRenderer_DrawSlotModel` (:222). The flame templates `D_800758D4..D_80075C10` are declared with real types at
:121-141 "so the lifter converts them to host data"; `src/port/generated/zero_lift_data.c:147-153` places them. There is
no runtime converter, so no ordering bug. The related Zero crash (second flame model, slot stride 0x7D0) was fixed
separately in plethron `d6648bb` (:179).

## How to detect
Crash in `GfxSpVertex` / G_VTX with an address that looks like two packed GBI words (e.g. `0x0000040606000204`): a
raw N64 display list reached the interpreter. Grep for one-time "Fix"/"Convert" helpers whose first caller is not the
only path to the data.
