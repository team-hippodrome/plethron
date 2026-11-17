# Unions that overlay a narrow field on a wider integer read the wrong half on a little-endian host (animation frame counter, GB APU phase)

- **Category:** endianness
- **Zero status:** already ported (plethron 4f2cad9 / PR #34 swapped AnimFixedFrame; plethron 5134413 added GBAPU_PHASE / GBAPU_SET_PHASE)
- **stadion:** no PR (initial port work and the ares comparison rounds, before PR #1; history squashed into `be110fe`). The later sweep of the same class is PR #61 (covered separately).
- **stadion files:** `game/src/model_animation.c`, `game/src/fragments/gb_tower_emulator/gb_tower_emulator_86CB0.c`, `game/include/color.h`, `game/src/matrix.c`

## Symptom
- Kids Club Sandshrew's Dig: the dirt mounds never grew. Any model animation played at a fractional speed (here 0x5000, i.e. 0.3125 frames per tick) never advanced or advanced wrongly.
- GB Tower sound: square/wave/noise channels played garbage pitch/duty (wrong waveform position).
- Earlier instances of the same class: cyan/green Pokémon models (red channel lost, `Color_RGBA8_u32`), matrices loaded wrong (`MtxF_ToFixed` stored s16 halves).

## Root cause
A union like `{ struct { s16 whole; s16 fraction; }; s32 raw; }` relies on big-endian layout: on the N64 the first u16/s16 is the HIGH half of the s32. On a little-endian host the first member is the LOW half. So:
- `AnimFixedFrame` (16.16 fixed animation frame): code adds `speedFixed` to `.raw` and then compares/clamps `.whole`; on the host `.whole` is the fraction.
- GB APU `phase` union `{ u16 index; s32 acc; }`: `acc` is a 21-bit accumulator, `index` must be its top 16 bits (`acc >> 16`), but reads the low 16 bits on the host.

## Fix in stadion
- `model_animation.c:265`: the struct halves are declared in host order (`s16 fraction; s16 whole;`) with a PORT comment.
- `gb_tower_emulator_86CB0.c:62`: explicit accessors instead of the union member:
```c
#define GBAPU_PHASE(ch) ((u16)((u32)(ch).phase.fixed >> 16))
#define GBAPU_SET_PHASE(ch, v) ((ch).phase.fixed = (s32)(((u32)(ch).phase.fixed & 0xFFFF) | ((u32)(u16)(v) << 16)))
```
- `color.h`: `Color_RGBA8_u32` byte names reversed under `__ORDER_LITTLE_ENDIAN__`; `matrix.c` `MtxF_ToFixed` builds packed 32-bit words.

## Zero / plethron
- **Needed:** `game/src/geo_render.c:41-47` declares `AnimFixedFrame { s16 whole; s16 fraction; }` (N64 order, no PORT note); `ModelAnim_AdvanceCurveFrame` (`geo_render.c:309-335`) does `spC.raw += arg0->speedFixed` and then reads/writes `ptr->whole`. Any Zero animation with a non-integer speed is affected. Swap the two members (as stadion `model_animation.c:265`).
- **Needed:** `game/src/fragments/audio_and_gb_emulator/audio_and_gb_emulator.h:333-336` union `{ u16 index; s32 acc; } phase`; `gb_apu.c:139/140, 237, 330, 400-403` read `phase.index` (and the noise channel writes it at 403). Port `GBAPU_PHASE` / `GBAPU_SET_PHASE`.
- Already ported: `GbApuRegs` bitfields (plethron `2740057`, `audio_and_gb_emulator.h:370ff`), `Color_RGBA8_u32` (`game/include/color.h:29-40`), `MtxF_ToFixed` (`game/src/matrix.c:721`, "PORT: from stadion").

## How to detect
- `tools/endian_union_sweep.py game/src` (in plethron) lists unions overlaying narrow fields on a wider integer, and casts reading narrow fields through wide types; review every "sensitive" use.
- Runtime: animations stuck on their first frame at fractional speeds; GB channels out of tune.
