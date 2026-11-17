# Memory leak: translated display lists never freed after a cache flush (~15 MB/s in battle)

- **Category:** performance
- **Zero status:** already ported (`src/port/gfx_translate.cpp:44-60`, `:571-579`)
- **stadion:** [PR #33](https://github.com/team-hippodrome/stadion/pull/33), commit(s) `c592e85`
- **stadion files:** `src/port/gfx_translate.cpp`, `src/port/Engine.cpp`, `src/port/sdk/ultra64.h`

## Symptom
Memory grew by ~15 MB/s in battle; several parallel games ran the machine out of memory.

## Root cause
`Port_GfxFlushCache` (called whenever the game changes its segment/fragment maps, constantly in battle) cleared only
the lookup tables; the arena holding translated lists, vertices and matrices was kept forever, because lists might
still be queued for the renderer and there was no frame fence.

## Fix in stadion
`RetireArena()` moves the arena to a retired queue on a flush or once it passes 32 MB; `Port_GfxFrameDone()` (called
after each rendered or tracked frame in `RunFrameWith`) frees retired arenas 4 frames later.

## Zero / plethron
`src/port/gfx_translate.cpp:44-60` (`gRetired`, `kArenaLimit`, `RetireArena`), `:571` (flush retires), `:575-579`
(`Port_GfxFrameDone`); called from `src/port/Engine.cpp:728` and `:764`.

## How to detect
Watch RSS during a long battle (`/proc/<pid>/status` VmRSS, or `top`); a steady climb means an arena/cache without a
release point. ASan/LSan at exit lists the growth sites.
