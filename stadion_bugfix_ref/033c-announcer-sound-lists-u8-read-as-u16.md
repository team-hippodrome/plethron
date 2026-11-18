# Castle crash: announcer sound lists were byte arrays in N64 order read as u16 (count read as 768)

- **Category:** endianness
- **Zero status:** not applicable (Zero's announcer has no such sound-list tables; data is lifted by type)
- **stadion:** [PR #33](https://github.com/team-hippodrome/stadion/pull/33), commit(s) `5c24a88`
- **stadion files:** `patches/game/02-core.patch` (`src/3D140_2.c`)

## Symptom
Crashes in `Audio_CreateFrameTask` during Gym Leader Castle battles; the audio manager was overwritten.

## Root cause
Five lists (`D_8007801C`, `D_80078024`, `D_80078030`, `D_80078038`, `D_80078070`: count, then sound ids) were declared
`u8[]` with bytes in N64 order but read as `u16*` by `Audio_SelectSoundAvoidingRecent` / `Audio_QueueSoundAvoidingRecent`.
On little-endian the count 0x0003 read as 0x0300, and the picker ran far past the list.

## Fix in stadion
Declare them `u16[]` with the values (`u16 D_80078070[4] = { 0x0003, 0x0071, 0x00A8, 0x00A9 };`).

## Zero / plethron
The Stadium-1 functions do not exist in Zero (`grep -rn "SoundAvoidingRecent" game/src`: only a prototype in
`fragments/registration/registration.h:114`). Zero's announcer (`game/src/audio_commands_4.c`) calls
`Announcer_QueueLine(<literal id>, ...)` and keeps state in u8 globals. Zero's rodata comes through the lifter, which
swaps scalars by the declared type, so a `u8[]` declaration read through `u16*` would show the same bug: check any
`extern u8 X[]` that is cast to `u16*`/`s16*`.

## How to detect
`grep -rn "(u16 \*)\|(u16\*)\|(s16\*)" game/src` on symbols declared as byte arrays; ASan overflow in audio code;
counts that are multiples of 256.
