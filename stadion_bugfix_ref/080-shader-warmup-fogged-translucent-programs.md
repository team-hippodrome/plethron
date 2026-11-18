# First Poke Ball throw stutter: fogged translucent shader programs were missing from the warm-up list

- **Category:** performance
- **Zero status:** already ported (plethron 8298280: the 14 programs Zero's 13 screen-diff routes built at first draw were appended to src/port/shader_warmup.txt)
- **stadion:** [PR #80](https://github.com/team-hippodrome/stadion/pull/80), commit(s) `db9371b` (squashed into `2db362c` locally)
- **stadion files:** `src/port/shader_warmup.txt`, `PORTING_NOTES.md`

## Symptom
On the first start of a freshly built app, the first Poke Ball throw (send-out in a fogged arena) hitched visibly.

## Root cause
Fast3D builds a shader program at the first draw that needs it; with an empty Metal cache that costs 500-700 ms.
The session's `shaders.txt` showed 8 programs not in the shipped list: ALPHA+FOG variants (some with texture
clamp/edge) of shipped programs, i.e. something translucent drawn in fog. No stored recording drew such a scene,
so `tools/shader_list.py` never collected them.

## Fix in stadion
Added the 8 program ids to `src/port/shader_warmup.txt` (121 programs; cold warm-up 13.4 s on worker threads during
boot). No code change.

## Zero / plethron
`src/port/shader_warmup.txt` is byte-identical to stadion's (125 lines; the PR's last id
`010d020d80000108 fffffffffffe0315` is at line 122), and `src/port/shader_warmup.cpp` is identical too, so the
mechanism and these entries are inherited. Whether Zero draws programs outside this list is unknown: the list was
collected from Stadium 1 recordings. To decide, play Zero's battles/menus once with a fresh settings folder and
compare the `shaders.txt` it writes next to the settings against `src/port/shader_warmup.txt`; ship the extra ids.

## How to detect
`PORT_TRACE_SHADERS=1` prints `[shader] <id0>/<id1> built in X ms` for every program built at its first draw (as
opposed to `prebuilt`). `PORT_SHADER_SALT` forces a cold Metal cache. Any `built` line during play is a stutter
that the warm-up list can remove.
