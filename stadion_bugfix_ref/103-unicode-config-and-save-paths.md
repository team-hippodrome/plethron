# Windows: settings and save files under non-ASCII paths (narrow fopen/ifstream on UTF-8 paths)

- **Category:** saves
- **Zero status:** needed (save/config paths are fixed, but plethron's own `gb_save_convert.cpp` still opens its store with plain `fopen`)
- **stadion:** [PR #103](https://github.com/team-hippodrome/stadion/pull/103), commit(s) `48c0515`, `6a0b802`, `1e07fc6` (merge `42a43df`)
- **stadion files:** `src/port/os_devices.cpp` (FlashLoad/FlashSave), `patches/libultraship.patch` (Config.cpp `fs::u8path`, Context.cpp app directory), `src/port/Engine.cpp` (SaveScreen size)

## Symptom
With the app (and so its settings and saves) in a folder with non-ASCII characters, Windows could not read or write
the JSON settings or the FlashRAM save.

## Root cause
Same as 088: UTF-8 path strings passed to narrow `fopen` / `std::ifstream(std::string)`, which Windows interprets in
the ANSI code page. Also in this PR: `SaveScreen` started from 0x0 when SDL had no window (DirectX) and the app
directory logic differed per platform.

## Fix in stadion
`FlashLoad`/`FlashSave` use `Port::OpenFileUtf8`; libultraship `Config::Reload/Save` open `fs::u8path(mPath)`;
`Context::GetAppDirectoryPath` uses one `ResolveAppDirectory` (portable folder next to the exe/.app/AppImage,
`SHIP_HOME` honoured); `SaveScreen` starts from the window backend's size.

## Zero / plethron
- Present: libultraship 4148f93 `src/ship/config/Config.cpp:203,208,221` use `fs::u8path(mPath)`;
  `src/port/os_devices.cpp:143` (flash) and `:182` (SRAM, Zero's save) open with `Port::OpenFileUtf8`; writes go
  through `Port::WriteProtectedSave(std::filesystem::u8path(...))` (`os_devices.cpp:156`, `:210`).
- Missing (plethron-only code): `src/port/gb_save_convert.cpp:244` `LoadStore()` does
  `FILE* f = fopen(StorePath().c_str(), "rb");` where `StorePath()` (`:217`) is the SRAM path + `.western`. On Windows
  with a non-ASCII save folder the Western-names store is written (`:264`, u8path) but never read back. Use
  `Port::OpenFileUtf8`.
- Other plain `fopen` in `os_devices.cpp:679,734,778,816` are replay/record paths from env vars (debug only).

## How to detect
`grep -n "fopen(\|ifstream\|ofstream" src/port/*.cpp src/port/save_editor/*.cpp` and check each path's origin.
Run once from a folder like `C:\Spiele\Pokémon` under Windows/Wine and check that every file written is read back.
