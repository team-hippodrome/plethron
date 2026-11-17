# Wrong ROM dumps started and crashed mid-game: check the ROM's SHA-1

- **Category:** platform-build
- **Zero status:** already ported (rom_check adapted to Zero's Japanese ROM)
- **stadion:** [PR #45](https://github.com/team-hippodrome/stadion/pull/45), commit(s) `52d46bb`
- **stadion files:** `src/port/rom_check.h`, `src/port/rom_check.cpp`, `src/port/launcher.cpp`, `src/port/main.cpp`, `src/port/Engine.cpp`

## Symptom
A Windows crash report: access violation in the ROM PI DMA (`Rom_DmaRead` -> `port_osEPiStartDma` copying to an address far outside game memory). Any dump with the right header (revision, patched/translated ROM, damaged file) started and crashed later. Also `osContInit` logged an error on every start because `gamecontrollerdb.txt` was missing.

## Root cause
The launcher only checked byte order, internal name and region; a ROM passed on the command line was not checked at all. The port's offset tables assume exactly one dump.

## Fix in stadion
SHA-1 check (launcher shows "Not the original dump" and keeps PLAY disabled; direct start shows both hashes in a message box); `PORT_ANY_ROM=1` skips it. A commented placeholder `gamecontrollerdb.txt` is created on first start.

## Zero / plethron
Present and retargeted to Zero: `src/port/rom_check.h:12-14` (`kRomSha1`/`kRomSize` for the 16 MiB Japanese dump), checks at `src/port/rom_check.cpp:136,153`, `PORT_ANY_ROM` at `rom_check.cpp:102`; controller DB placeholder in `src/port/Engine.cpp:151-160`.

## How to detect
Crashes in DMA/asset reads whose ROM offsets look plausible but data does not: compare the user's ROM hash first. grep for hard-coded ROM offsets/sizes whenever the expected dump changes.
