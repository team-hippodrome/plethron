# macOS-only assumptions: "N64 address = below 4 GB", 47-bit vertex check, Mach-O symbol prefixes and sections

- **Category:** platform-build
- **Zero status:** already ported (`Port_ReserveLowAddressSpace`, 48-bit check in the fork, per-platform symbol and section code)
- **stadion:** no PR ("Linux and Windows", before PR #1; history squashed into `be110fe`)
- **stadion files:** `src/port/port_platform.c`, `src/port/main.cpp`, libultraship `src/fast/interpreter.cpp`, `tools/gen_abs_syms.py`, `tools/gen_func_table.py`, `tools/gen_frag_sections.py`, `tools/gen_word_arrays.py`

## Symptom
On Linux/Windows: valid heap vertex arrays rejected (arm64 Linux heap `0xaaaa...`, above 47 bits); host pointers on Windows below 4 GB taken for N64 segment/fragment addresses; link failures from `_`-prefixed symbols; per-fragment data sections and word-array lookup (`dladdr` only sees exported symbols) not working; libultraship's crash handler replaced the port's.

## Root cause
The port tells N64 addresses from host pointers by "below 4 GB" (true on macOS/Linux 64-bit, not guaranteed on Windows), and the first versions used Mach-O-only features.

## Fix in stadion
`Port_ReserveLowAddressSpace` reserves the free space below 4 GB at start on Win64; vertex check upper limit = 48-bit user space; `SYM()` / `PORT_ASM_NAME` pick the symbol prefix; ELF `__start_/__stop_` and COFF `$a/$m/$z` sections; word arrays registered through generated wrappers (`Port_RegisterWordArray`); port crash handler installed after `Port::Init`, alternate signal stacks; lld on Linux.

## Zero / plethron
Already ported: `src/port/port_platform.c:8-11` (`Port_ReserveLowAddressSpace`, Win64), called at `src/port/main.cpp:394`; fork 4148f93 `src/fast/interpreter.cpp:5065` (`va >= 0x1000000000000ULL`, "user space is 48-bit"); `tools/gen_frag_sections.py` COFF/ELF branches; plethron `f7db975` (MinGW COFF aliases) and `6500766` (Windows bitfields) are its own follow-ups.

## How to detect
Run replays on all three OSes; `[badvtx]` / "bad address" logs that appear only on one OS; heap addresses printed with `PORT_ALLOCLOG`.
