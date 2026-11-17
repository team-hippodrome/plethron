# Smooth motion: menus stayed at 30 fps because setting the same segment map flushed the DL cache every frame

- **Category:** timing-pacing
- **Zero status:** already ported (plethron 0f590be: `Memmap_SetSegmentMap` flushes only on a change)
- **stadion:** [PR #120](https://github.com/team-hippodrome/stadion/pull/120), commit(s) `605a4ed` (merge `1c00cf4`)
- **stadion files:** `patches/game/02-core.patch` (src/memmap.c `Memmap_SetSegmentMap`)

## Symptom
With Smooth motion, menus and the rental screen drew ~310 frames per 10 s (30 fps) instead of ~575.

## Root cause
Menus call `Memmap_SetSegmentMap` with the same values every frame. Each call flushed the display-list translation
cache (`Port_GfxFlushCache`), and that flush also tells Smooth motion that game data moved, which drops the pending
in-between frame. The fast test mode drew in-between frames at once and did not show it.

## Fix in stadion
Flush only when the map changes:
```c
if (gSegments[id].vaddr != vaddr || gSegments[id].size != size) { Port_GfxFlushCache(); }
```

## Zero / plethron
`game/src/memmap.c:445-453` has exactly this check, with the comment "Menus set the same map again every frame:
only a change flushes". `git log -S` dates it to plethron `0f590be`.

## How to detect
`PORT_TRACE_PACING=1` in real time on a static menu: drawn frames per 10 s should be ~600 with Smooth motion.
Count `Port_GfxFlushCache` calls per frame; more than zero on an unchanged screen is suspicious.
