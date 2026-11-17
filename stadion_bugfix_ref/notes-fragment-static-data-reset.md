# Statically linked fragments kept their globals between visits (area select came back black)

- **Category:** game-logic
- **Zero status:** already ported (`Port_FragmentResetData` called from Zero's fragment loader)
- **stadion:** no PR (third ares round, before PR #1; history squashed into `be110fe`; web build follow-up `9978eec`)
- **stadion files:** `tools/gen_frag_sections.py`, `CMakeLists.txt`, `game/src/jpeg_stream.c` (Fragment_Load)

## Symptom
After leaving an area (e.g. the Lab) the hub/area select came back black: the cursor glow still held the closing iris (alpha 0xFF) from the previous visit.

## Root cause
On the N64 every fragment load DMAs the fragment's code and .data from ROM and clears its .bss, so each visit starts from the ROM image. The port links all fragment code statically: globals and function statics keep whatever the previous visit left.

## Fix in stadion
`tools/gen_frag_sections.py` puts each fragment directory's writable data in its own section (Mach-O `__DATA,__fdNN`, ELF named sections with `__start_/__stop_`, COFF `$a/$m/$z` groups), snapshots them at startup and restores .data / clears .bss in `Fragment_Load`. `PORT_NO_FRAG_RESET=1` turns it off, `PORT_TRACE_FRAG_RESET=1` logs it. ASan: the copies are uninstrumented byte loops.

## Zero / plethron
Already ported: `CMakeLists.txt:287-322` runs `tools/gen_frag_sections.py`; `game/src/port_fragments.c:65-82` calls `Port_FragmentResetData(e->entry)` on every load, and notes that the asm-side lifted data is reset by the data lifter (`src/port/zero_lift.c`). `src/port/StadionPort.h:159` declares it. `tools/test_frag_sections.py` exists.

## How to detect
A screen that is right on the first visit and wrong on the second; `PORT_NO_FRAG_RESET=1` makes it reproducible. New fragment directories must have an entry in `src/port/generated/port_fragment_entries.c` to be covered.
