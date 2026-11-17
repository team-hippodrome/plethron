# Windows crash 0x20474343 at start with a damaged or missing pokestadium.o2r: archive built into the exe

- **Category:** platform-build
- **Zero status:** already ported (plethron mounts the built-in archive `gPortO2r`)
- **stadion:** [PR #82](https://github.com/team-hippodrome/stadion/pull/82), commit(s) `972c4a4` (squashed into `2db362c` locally)
- **stadion files:** `src/port/Engine.cpp`, `tools/make_o2r.py`, `CMakeLists.txt`, `patches/libultraship.patch` (O2rArchive, ResourceManager::SetBuiltInArchive), packaging scripts

## Symptom
A Windows install crashed at start with `Exception: 0x20474343` after `Failed to load zip file ...pokestadium.o2r`
/ `ResourceManager init deferred`.

## Root cause
`Port::Init` caught the archive failure and continued without the archive; the renderers build their shaders from
the archive, so the game crashed a moment later. A missing file was no better: libultraship opened archives with
`ZIP_CREATE`, so a missing file opened as an empty archive without error.

## Fix in stadion
`tools/make_o2r.py --c` emits the packed archive as a C array (`generated/pokestadium_o2r.c`) compiled into the
game; libultraship's `O2rArchive` can open a ZIP image in memory (`zip_source_buffer_create` +
`zip_open_from_source`) and `ResourceManager::SetBuiltInArchive` mounts it before any archive on disk. A file next
to the exe is no longer read; if the built-in archive fails, a message box and exit instead of a later crash.

## Zero / plethron
`src/port/Engine.cpp:170-171` declares `gPortO2r`/`gPortO2rSize` and `Engine.cpp:304` calls
`resourceManager->SetBuiltInArchive("pokestadium.o2r (built in)", gPortO2r, gPortO2rSize)`. The pinned
libultraship (4148f93) has `ResourceManager::SetBuiltInArchive` (`src/ship/resource/ResourceManager.cpp:55`).

## How to detect
Put a corrupt `pokestadium.o2r` next to the exe and start: the game must ignore it. Grep for `ZIP_CREATE` in archive
open paths (a read-only archive should never be created silently) and for catch blocks that "defer" init.
