# Same memory read through two struct types whose fields agree only on the N64 layout (layout checker)

- **Category:** abi-64bit
- **Zero status:** already ported (plethron has the checker and typed most views; re-run it after decomp syncs)
- **stadion:** no PR ("Struct layout checker", Lab PC widget pass, ares rounds; before PR #1, history squashed into `be110fe`). Lab PC list views fixed later in PRs #64, #68-#70 are covered separately.
- **stadion files:** `tools/layout_check.py`, `tools/mips_sysroot`, many game headers

## Symptom
Wrong fields read with no crash, or callbacks called through padding: Yellow's partner Pikachu with garbage textures (palette descriptor read at host +16 instead of the converted descriptor's +8, so no LOADTLUT ran: every paletted model affected); Gallery photo camera without a view (eye/at written at N64 offset 0xA8); trainer panel viewport fields misread (`PresentationLayout`); battle-effect node display list overwriting the GraphNode callback; `LabPC_ReleaseSwapWidgets` and sort comparators reading 8 bytes early; save progress moved by a pointer in `GameOptions`.

## Root cause
The decomp often has partial or char-padded "view" structs (`char unk[0x10]; void (*cb)(...)`) for memory that is really another type. They line up on the N64 (4-byte pointers), not on the host, where every field after a pointer moves.

## Fix in stadion
`tools/layout_check.py`: clang `-fdump-record-layouts` for the host and for 32-bit big-endian MIPS, clang `-ast-dump` for every conversion between two struct pointer types, and for each pair every N64 offset where both start a field must map to the same host offset. Views were replaced by the real types (`WidgetLinkHeader` first member, `{ GraphNode node; Gfx* unk_18; }`, `unk_60.eye`, typed descriptor pointer). Also the RDP behaviour for CI textures with TLUT off.

## Zero / plethron
Already ported: plethron has `tools/layout_check.py` (same tool, 248 lines) and `tools/mips_sysroot`; Zero's geo/texture code is typed (`game/src/geo_render.c:761` "typed fields (were words 3, 4 and 5 of the state)", `:868` typed rewrite of s8* offset reads, `:676` `D_80060488->view.unk_44` instead of `M2C_FIELD(..., 0xA4)`). plethron's `6500766` fixed Windows bitfield/struct-by-value layout. Lab PC/Pokédex in Zero are different screens; not re-verified pair by pair here.

## How to detect
`tools/layout_check.py` → `build/layout/report.txt` (mismatching pairs with conversion sites) and `comment_errors.txt`. grep `M2C_FIELD(`, `char unk_\w+\[0x[0-9A-F]+\];` before a pointer, `(s8*)x + 0x`.
