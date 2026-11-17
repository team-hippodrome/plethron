# Windows: ROM under a non-ASCII path could not be validated or loaded (narrow fopen on a UTF-8 path)

- **Category:** platform-build
- **Zero status:** already ported (`Port::OpenFileUtf8` is used for the ROM check and the loader)
- **stadion:** [PR #88](https://github.com/team-hippodrome/stadion/pull/88), commit(s) `d58db98` (squashed into `2db362c` locally)
- **stadion files:** `src/port/file_io.h` (new), `src/port/rom_check.cpp`, `src/port/os_devices.cpp`, `tools/test_rom_paths.cpp`

## Symptom
Selecting a valid ROM under an accented/CJK path left Play disabled with "Cannot open the file".

## Root cause
SDL file dialogs and the launcher store UTF-8 paths; the header read used `std::filesystem::u8path`, but the SHA-1
check and `Port::LoadRom` passed the UTF-8 string to narrow `fopen`, which on Windows uses the ANSI code page.

## Fix in stadion
`Port::OpenFileUtf8(path, mode)`: on Windows `_wfopen(std::filesystem::u8path(path).c_str(), wide mode)`, elsewhere
`fopen`. Used for `Sha1HexFile`, `CheckRomDumpFile` and `Port::LoadRom`.

## Zero / plethron
`src/port/file_io.h:13-17` has `OpenFileUtf8` with `_wfopen`; `src/port/rom_check.cpp:117` and `:146` and
`src/port/os_devices.cpp:67` (`Port::LoadRom`) use it. See also 103 for a remaining plain `fopen` on a save path.

## How to detect
`grep -n "fopen(" src/port/*.cpp` and check every hit whose path came from the user, the launcher config or
`AppPath`. Test with a ROM/save in a folder like `José/桌面` under Wine or Windows (`tools/test_rom_paths.cpp` in
stadion).
