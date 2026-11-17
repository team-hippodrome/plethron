# Texture cache: rewritten RAM buffers piled every version into one bucket chain (window-thread CPU)

- **Category:** performance
- **Zero status:** already ported (fork at 4148f93: content hash in `ImportTexture`, interpreter.cpp:1862)
- **stadion:** [PR #34](https://github.com/team-hippodrome/stadion/pull/34) (follow-up in #36, outside this slice), commit(s) `6288381`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`)

## Symptom
7.5% of the window thread spent walking texture cache buckets; battle stutter on the AYN Thor.

## Root cause
The cache was keyed by RAM address; buffers the game rewrites (fonts, backgrounds, render targets) kept every past
version under the same address, so lookups walked ever-longer chains.

## Fix in stadion
Hash the texel content (and palette in use) into the key; `TextureCacheDelete` scans the whole cache (the game never
sends that opcode).

## Zero / plethron
`git -C /home/user/lus show 4148f93:src/fast/interpreter.cpp`: "content hash (FNV-1a over the texels and the palette
in use)" at line 1862, `TextureCacheDelete` at 999.

## How to detect
Profile the window thread: time in `TextureCacheLookup` / unordered_map bucket walks growing over a session.
