# Large Pokémon lost their last graph nodes (Raichu's head): model buffers too small for 64-bit nodes

- **Category:** abi-64bit
- **Zero status:** already ported (`game/src/display_object_textures.c:488`, `MODEL_BUFFER_SIZE (0x3C000 * 2)`)
- **stadion:** [PR #28](https://github.com/team-hippodrome/stadion/pull/28) (to `autoplay`), brought to main by [PR #33](https://github.com/team-hippodrome/stadion/pull/33), commit(s) `f709b44` / `832d380`
- **stadion files:** `patches/game/02-core.patch` (`src/display_object_textures.c`)

## Symptom
Raichu's face rendered as a hole into its body (rental carousel, battles); Dodrio lost its heads, Pidgeotto its legs,
Kangaskhan its feet, Pidgeot and Hypno small parts.

## Root cause
`PokeIcon_AllocFramebuffers` gives each loaded model a 0x3C000-byte buffer holding the model file and then the graph
nodes built from its geo layouts. On the host those nodes are larger (64-bit pointers), so the largest models ran out
of room; `MainPool_AllocAligned` returned NULL and the remaining nodes were silently dropped (69 failed allocations
for Raichu: the head joint's subtree).

## Fix in stadion
`#define MODEL_BUFFER_SIZE (0x3C000 * 2)` for all model-buffer allocations and for the size passed to the loader thread.

## Zero / plethron
Present: `game/src/display_object_textures.c:486-488` defines `MODEL_BUFFER_SIZE (0x3C000 * 2)` ("from stadion"),
used at `:503`, `:505` and as the loader request size at `:548` (came in with plethron `7e758dd`).

## How to detect
Log NULL returns from the pool allocators used by geo layout processing (any silent `if (p == NULL) return;`), or
diff all rental/Pokédex models against an emulator. Grep fixed N64 buffer sizes that hold host structs:
`grep -rn "MainPool_Alloc(0x" game/src`.
