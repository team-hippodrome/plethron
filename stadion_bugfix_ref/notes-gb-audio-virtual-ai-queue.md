# GB sound ran 1.5 % slow with host output off: osAiGetLength reported an empty queue

- **Category:** audio
- **Zero status:** already ported (`src/port/gb_audio_out.cpp` is identical to stadion's)
- **stadion:** no PR (German Yellow vs ares round, before PR #1; history squashed into `be110fe`)
- **stadion files:** `src/port/gb_audio_out.cpp`

## Symptom
GB Tower sound dumps (`PORT_GB_WAV`, made with host sound off: replays, `PORT_SPEED=max`) drifted behind ares: 0.8 s after 55 s.

## Root cause
The tower's mixer sizes each buffer from `osAiGetLength` (`len = ((800 - aiLen + 0x80) & ~15) + 16`, clamped 784..816). With output off the port reported an empty AI queue, so every buffer got 816 samples instead of the ~802.6 per VI frame the N64 AI drains.

## Fix in stadion
With the output off, a virtual AI queue drained by VI ticks at the DAC rate (clock 48681812 / round(clock / 48000)).

## Zero / plethron
Already ported: plethron's `src/port/gb_audio_out.cpp` is byte-identical to stadion's apart from whitespace (lines 9 and 28 describe the virtual queue). Zero's GB Player submits through `Port_GbAudioSubmit` (`game/src/fragments/audio_and_gb_emulator/gb_set_next_buffer.c:58`).

## How to detect
`tools/audio_compare.py` against an ares `ARES_REF_WAV` recording: a steady growing offset means the produced sample count per frame is wrong.
