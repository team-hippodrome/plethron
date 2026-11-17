# Music / Sound effects sliders followed slider^4 (50 % = -24 dB, 25 % silent)

- **Category:** audio
- **Zero status:** already ported (plethron b1ea1c8: `Audio_ComputeVoiceVolume` applies the Music / Sound effects / Voices gains)
- **stadion:** [PR #137](https://github.com/team-hippodrome/stadion/pull/137), commit(s) `29276a6` (hook from [PR #126](https://github.com/team-hippodrome/stadion/pull/126) `fc77ede`)
- **stadion files:** `src/port/sound_settings.cpp`, `src/port/sound_settings.h`, `game/src/libnumus/player.c`

## Symptom
Lowering Music made it inaudible: 50 % was -24 dB, 25 % silent.

## Root cause
The launcher gain was applied to the music player's per-voice volume in `Audio_ComputeVoiceVolume`
(libnumus/player.c), which the synthesizer squares again (libnaudio `n_env.c`: `em_volume = volume^2 >> 15`). The
slider's square curve on top of that gave slider^4.

## Fix in stadion
`Port_SoundPartGainQ15` returns a linear gain for `PORT_SOUND_MUSIC` / `PORT_SOUND_EFFECTS` (amplitude ends up
slider^2, like Voices and Master):
```c
if (part == PORT_SOUND_MUSIC || part == PORT_SOUND_EFFECTS) return (v * 32768) / 100;
```
The game side (#126) multiplies the voice volume by it in `Audio_ComputeVoiceVolume` after the master volume.

## Zero / plethron
- Curve: present, `src/port/sound_settings.cpp:39` (file identical to stadion).
- Hook: missing. `grep -rn Port_SoundPartGainQ15 game/` finds nothing. Zero's
  `game/src/libnumus/player.c:1772` `Audio_ComputeVoiceVolume` ends after `mus_master_volume_songs/effects` and the
  duration scale with no `Port_SoundPartGainQ15` call (stadion: `game/src/libnumus/player.c:1951-1956`). The launcher
  still shows Music, Sound effects and Voices sliders (`src/port/launcher.cpp:1642-1644`). Master / mute work (they
  are SDL stream gain in `audio_out.cpp`, identical to stadion).
- To do: add the stadion block after `volume >>= 0xF;` (`player.c:1786`) in Zero's `Audio_ComputeVoiceVolume`; for Voices find where
  Zero plays cries/announcer (stadion uses `audio_stream.c:182`; Zero has no `audio_stream.c`) or hide that slider.

## Fixed in plethron (b1ea1c8)
`game/src/libnumus/player.c` `Audio_ComputeVoiceVolume` scales the voice volume by `Port_SoundPartGainQ15(Audio_PortSoundPart(cp))`. Zero has no `audio_stream.c`: cries (bank `D_800B4190`, `Audio_PlayCry`) and the announcer (bank `D_800B418C`, JIKKYOU) are music-player effects, so `Audio_PortSoundPart` sends them to Voices, and plethron's `Port_SoundPartGainQ15` returns the linear gain for Voices too (the synthesizer squares the voice volume, `audio_manager.c`). `PORT_AUDIO_DUMP` over boot, intro and the title demo battle with each slider at 0: Music 0 silences the songs (e.g. 5075 to 12 RMS), Sound effects 0 the effects, Voices 0 the send-out cries; all at 100 gives the same samples as before.

## How to detect
`PORT_AUDIO_DUMP` with a slider at 0 vs 100 (dump must change); `grep -rn Port_SoundPartGainQ15 game/`.
