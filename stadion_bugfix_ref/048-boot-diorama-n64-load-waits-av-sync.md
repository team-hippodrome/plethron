# Boot intro out of sync with its music: the N64's synchronous model loads were instant

- **Category:** timing-pacing
- **Zero status:** not applicable (Zero has no boot diorama; the port-side switch exists but nothing calls it)
- **stadion:** [PR #48](https://github.com/team-hippodrome/stadion/pull/48), commit(s) `5d69c51`
- **stadion files:** `patches/game` (`demo_scenes_15FA60.c`, `Title_Main`), `game/src/port_audio.c` (`Port_LoadStall`), `src/port/os_devices.cpp`

## Symptom
Testers: "the intro was unsync'd". The boot diorama's pictures ran ahead of the music by up to 54 frames (~0.9 s) at the last model group; the title music came ~340 ms early.

## Root cause
On the N64 `Diorama_SwitchModelGroup` loads each group's models synchronously, stalling the game loop 5-21 retraces while the music plays on; the music is timed around those stalls. The port loads instantly. Title_Main similarly took ~20 retraces more on the N64 before starting its music.

## Fix in stadion
After each group's load the game thread blocks on an `osSetTimer` for the retraces the load took on the N64 (`Port_LoadStall`), and `Title_Main` waits before starting the music. Recordings get a `# load waits` line; `PORT_LOAD_WAITS=0/1` overrides.

## Zero / plethron
Zero boots N64 logo (`game/src/fragments/n64_logo/`) then title (`game/src/fragments/title_screen/`); there is no `demo_scenes` diorama (pokestadium0 `docs/layout.md` fragment list). The port side came along: `src/port/os_devices.cpp:704-706` (`Port_LoadWaits`, `PORT_LOAD_WAITS`) and the `# load waits` recording line at `:821-822`, but no `Port_LoadStall` exists and nothing in `game/` calls a load wait. If Zero's title music start is ever measured early against a console capture, the same technique applies to Zero's title load.

## How to detect
`tools/av_sync.py` (picture-to-sound offset of a run against a video) or `PORT_AUDIO_DUMP` vs ares: an A/V offset that steps up at each scene/model load means the N64 stalled there and the port did not.
