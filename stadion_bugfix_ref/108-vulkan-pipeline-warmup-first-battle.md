# Vulkan stutter at first battle/scene: pipeline states built at first draw (warm-up list)

- **Category:** performance
- **Zero status:** check (the list file is inherited byte-identical; its entries come from Stadium 1 recordings)
- **stadion:** [PR #108](https://github.com/team-hippodrome/stadion/pull/108), commit(s) `56cf3e9`; follow-ups [#109](https://github.com/team-hippodrome/stadion/pull/109) `8215ba3`, [#111](https://github.com/team-hippodrome/stadion/pull/111) `5b7d143`, [#110](https://github.com/team-hippodrome/stadion/pull/110) `543ed0b`
- **stadion files:** `src/port/vulkan_pipeline_warmup.txt`, `patches/libultraship.patch`, `tools/make_o2r.py`, `tools/shader_list.py`, `tools/shader_source_list.py`

## Symptom
Hitches in the first battle (and other first-time scenes) on Vulkan, after the GL/Metal shader warm-up was in place.

## Root cause
Vulkan pipelines depend on more than the shader program (blend, depth, topology, MSAA); each new combination is
compiled at first draw. The program warm-up list did not cover these states.

## Fix in stadion
A separate list of pipeline keys (`src/port/vulkan_pipeline_warmup.txt`) packed into the archive and pre-built at
start (libultraship patch). #109/#110/#111 extended it to more scenes (Strength, Solar Beam charge) and added
`tools/shader_source_list.py` to verify coverage from the sources.

## Zero / plethron
`src/port/vulkan_pipeline_warmup.txt` is identical to stadion's (138 lines, `diff -q` clean), so the mechanism and
Stadium 1's entries are inherited. Zero-specific scenes are not covered unless they happen to match. Record a Zero
session on Vulkan with a fresh settings folder, compare the recorded pipelines with the shipped list, add the rest.

## How to detect
`PORT_TRACE_SHADERS=1` / `PORT_TRACE_PACING=1` on Vulkan during first play; any pipeline built at draw time is a
stutter candidate. stadion's `tools/shader_list.py` and `tools/shader_source_list.py` show the method.
