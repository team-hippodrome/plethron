# CPU wasted in getenv: debug switches read on every call in per-frame/per-command code

- **Category:** performance
- **Zero status:** already ported (static caches in src/port and the fork; `PORT_GETENV_ONCE` in `src/port/StadionPort.h:179`)
- **stadion:** [PR #29](https://github.com/team-hippodrome/stadion/pull/29), commit(s) `217d0fb`
- **stadion files:** `src/port/gfx_translate.cpp`, `src/port/Engine.cpp`, `src/port/os_thread.cpp`, `src/port/os_devices.cpp`, `patches/libultraship.patch`, `patches/game/*` (`port_compat.h`, `Font_DrawChar`, geo renderer, battle HUD, ...)

## Symptom
At max speed `__findenv_locked` was the largest item of real CPU work in profiles (18 samples, ahead of game code);
noticeable on weak devices.

## Root cause
`getenv` scans the whole environment. Debug switches (`PORT_VALIDATE_DL`, `PORT_MTXLOG`, `PORT_TEXLOG`,
`PORT_TRACE_*`, ...) were checked per frame, per display-list command, per matrix and per message.

## Fix in stadion
`static const bool x = getenv(...) != nullptr;` in C++ port code and libultraship; a `PORT_GETENV_ONCE(name)`
statement-expression macro for game C code.

## Zero / plethron
Port: `src/port/gfx_translate.cpp:71,75` (static caches). Fork: e.g. `PORT_TILE1LOG` cached at interpreter.cpp
lines 182/2636/3436/5481 at 4148f93. Game: `PORT_GETENV_ONCE` defined in `src/port/StadionPort.h:179`, used in
`game/src/memmap.c:91,103`. The remaining raw `getenv` calls in `game/src` (`grep -rn "getenv(" game/src`: 5 hits)
are one-shot or cached (`memmap.c:219`, `game_state.c:499`, `subtitles.c:185`, `port_fragments.c:71`,
`audio_music.c:121`).

## How to detect
`grep -rn "getenv(" src game/src | grep -v static` and check each hit is not in a hot path; profile with `perf`/`sample`
for `getenv`/`__findenv`.
