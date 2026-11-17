# Geo fog colour sent in memory byte order (pale yellow instead of blue fog)

- **Category:** endianness
- **Zero status:** not applicable (Zero stores and emits the fog colour numerically, consistently)
- **stadion:** [PR #51](https://github.com/team-hippodrome/stadion/pull/51), commit(s) `dfe6826`
- **stadion files:** `patches/game/02-core.patch` (`src/geo_render.c` `Geo_NodeFog`, `GeoRender_SetRenderMode`)

## Symptom
The Pokédex AREA map had no blue fog tint: fog 0x96C8FF arrived as pale yellow (FF,FF,C8). The fix also changed the gallery photo's sky.

## Root cause
stadion's `GeoNode_CreateFog` stores `.rgba` as the bytes r,g,b,a in memory order (`PORT_BE32` of the numeric value) because other code reads it bytewise, but `Geo_NodeFog` and `GeoRender_SetRenderMode` passed that word unswapped to `G_SETFOGCOLOR`, which takes 0xRRGGBBAA.

## Fix in stadion
`gDPSetColor(..., G_SETFOGCOLOR, PORT_BE32(...))` at both sites (`geo_render.c:629`, `:1206` today).

## Zero / plethron
Zero keeps the numeric form end to end: `game/src/geo_node.c:259` (`fog.color.rgba = (r << 0x18) | (g << 0x10) | (b << 8) | a;`) and `game/src/geo_render.c:1221` emits it unchanged. The one bytewise consumer, the light colour, swaps explicitly: `geo_render.c:1244-1245` (`PORT_BE32(arg->color.rgba)`). No change needed unless someone adds a swap in `GeoNode_CreateFog`.

## How to detect
grep every writer and reader of a colour union (`.rgba` vs `.r/.g/.b`) and check they agree on numeric vs byte order; compare a fogged scene's colour with ares.
