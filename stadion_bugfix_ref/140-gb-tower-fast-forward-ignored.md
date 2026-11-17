# GB Tower Doduo/Dodrio fast-forward had no effect (native GB core always ran one frame)

- **Category:** timing-pacing
- **Zero status:** already ported (Zero's GB Player loops the core 1/2/4 times on the game side; plethron `bb27580`)
- **stadion:** [PR #140](https://github.com/team-hippodrome/stadion/pull/140), commit(s) `ea1a8db`
- **stadion files:** `src/port/gb_core.c`

## Symptom
C-Right showed the speed icons but the Game Boy game kept running at 1x.

## Root cause
The tower passes `speed * 154` lines to `GbCpu_Execute`; the original MIPS emulator runs that budget. The port's
native core ignored the argument and always ran exactly one frame.

## Fix in stadion
`GbCpu_Execute` runs `cycles / 154` frames (clamped 1..4), each with its own frame end; APU write time stamps run over
the whole call divided by the speed, so the music speeds up too.

## Zero / plethron
Different structure, fix not needed: Zero's `GbEmu_UpdateFrame`
(`game/src/fragments/audio_and_gb_emulator/gb_emu_frame.c:256-272`) itself loops `count` times calling `GbCpu_Run`,
setting the sound time base at `GB_MEM + 0x10100` per frame; `src/port/gb_core.c:1219` `Port_GbCoreRun` runs one
Game Boy frame per call, and `gb_core.c:362-371` stamps APU writes as `(base + cycles) / speed`, as the original.

## How to detect
Count Game Boy frames per N64 frame at each speed (`PORT_TRACE_GB=1` prints the GB frame counter every 30 frames).
