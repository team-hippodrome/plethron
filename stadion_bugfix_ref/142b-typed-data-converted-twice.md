# Flame column invisible/grey: a runtime N64-to-host converter ran over data the decomp already typed

- **Category:** endianness
- **Zero status:** not applicable (plethron has no runtime template converter; the lifter types the flame tables once)
- **stadion:** [PR #142](https://github.com/team-hippodrome/stadion/pull/142), commit(s) `7bfafe2`
- **stadion files:** `game/src/33FE0.c`, `game/src/33FE0.h`, `game/src/model_renderer_callbacks.c`

## Symptom
After vendoring the newer decomp, the flame column on Moltres's head (slot-model templates) was invisible or grey.

## Root cause
The port kept the templates as raw N64 u32 words and converted them on first use (`Port_FixSlotTemplates`, see
`131-...`). The new decomp defines them as `Vtx`, `Gfx` and `s16` arrays, i.e. already native, so the converter
byte-swapped vertex fields again and rebuilt the display list from 32-bit halves of host `Gfx` commands.

## Fix in stadion
Converter and its calls removed.

## Zero / plethron
`game/src/model_renderer_callbacks.c:121-141` declares the flame tables typed for the lifter
(`src/port/generated/zero_lift_data.c:147-153`); `D_80075998` / `D_80075B40` are deliberately kept as `u8` N64 display
lists "for the display-list translation". `grep -rn "Port_Fix" game/src` finds no converter. Watch for the same class
whenever a lifted symbol's declared type changes (typed by the lifter AND swapped by hand = double conversion).

## How to detect
When re-typing a symbol (`u8[]` → `Vtx[]`/`Gfx[]`), grep its name for `Port_Swap*`, `Port_ReadBE*` or translate
calls. Symptom: geometry/textures that look byte-swapped only after a decomp sync.
