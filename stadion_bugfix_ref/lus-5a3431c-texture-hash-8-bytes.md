# Texture cache key hashed every imported texture one byte at a time (FNV-1a, ~19 us per 4 KB)

- **Category:** performance
- **Zero status:** already ported (plethron 4db7329: libultraship pin 95a0f1f, the fork's main)
- **stadion:** [PR #152](https://github.com/team-hippodrome/stadion/pull/152), commit(s) `e75ec0b` (libultraship `5a3431c`, fork PR #5)
- **stadion files:** `libultraship` (`src/fast/interpreter.cpp`)

## Symptom
After the event pump, the window thread's largest single cost; a 300-tile background upload took 4.0 ms.

## Root cause
`ImportTexture`'s content hash ran FNV-1a over texels and palette, one dependent multiply per byte, on every import,
over up to 1 MB.

## Fix in stadion
`HashBytes`: xxHash64's four-lane stripe loop, 32 bytes per step (10-30x faster). Cup replay 16.7 G → 16.0-16.2 G
instructions; the 300-tile upload 4.0 ms → 2.1 ms. Pictures byte-identical.

## Zero / plethron
plethron's pin `4148f93`: `src/fast/interpreter.cpp:1862-1866`
(`for (size_t k = 0; k < n; k++) h = (h ^ p[k]) * 1099511628211ull;`). Bump the submodule to `8556b99`.

## How to detect
Profile the window thread (`ImportTexture` near the top); `PORT_TRACE_FRAMETIME` (in the fork since #3) for CPU time
per picture.
