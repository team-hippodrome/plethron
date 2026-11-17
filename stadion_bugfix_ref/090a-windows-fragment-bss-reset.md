# Windows: fragment data/BSS were not reset when a fragment was loaded again (stale hub fade)

- **Category:** platform-build
- **Zero status:** already ported (plethron's `tools/gen_frag_sections.py` emits `.data$fdNN$`/`.bss$fbNN$` sections)
- **stadion:** [PR #90](https://github.com/team-hippodrome/stadion/pull/90), commit(s) `34201be` (squashed into `2db362c` locally)
- **stadion files:** `tools/gen_frag_sections.py`, `tools/test_frag_sections.py`

## Symptom
On Windows, returning from Free Battle to the hub kept the hub's fade state from the previous visit (stale globals).

## Root cause
The port emulates N64 overlay loading by restoring each fragment's initialized data and zeroing its BSS on load
(`Port_FragmentResetData`). On Windows the generator used MSVC `#pragma data_seg/bss_seg`, which clang ignores, and
the DATA boundary markers could not bound BSS (COFF only merges sections with matching characteristics), so the
reset touched nothing.

## Fix in stadion
`#pragma clang section data=".data$fdNN$m" bss=".bss$fbNN$m"`; DATA markers via `__attribute__((section(...)))`,
BSS markers via `__asm__(".section .bss$fbNN$a,\"bw\" ...")` so they sort around the fragment's BSS. Plus a ROM-free
test that compiles two fake fragments and checks reset, isolation and the PE section count.

## Zero / plethron
`tools/gen_frag_sections.py:34` builds the `.bss$...,"bw"` assembly markers and `:103` emits
`#pragma clang section data=".data$fd%02d$m" bss=".bss$fb%02d$m"`. plethron's copy only adds an Emscripten branch
(diff against stadion's version).

## How to detect
Run `tools/test_frag_sections.py` with the Windows clang. Runtime: visit a mode twice and compare; state that should
start fresh (fades, cursors, counters) but keeps its old value points at the fragment reset.
