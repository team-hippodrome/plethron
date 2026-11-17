# Game threads must use the FPU rounding mode the game sets (__osSetFpcCsr was a stub)

- **Category:** game-logic
- **Zero status:** already ported (inherited `src/port/os_devices.cpp`; Zero asks for round-to-nearest, so it is a no-op there)
- **stadion:** [PR #59](https://github.com/team-hippodrome/stadion/pull/59), commit(s) `c20cfd7`
- **stadion files:** `src/port/os_devices.cpp` (`__osSetFpcCsr`), `src/port/os_thread.cpp`

## Symptom
Ice Beam's pulsing mouth flare drifted out of phase with ares (period 11 game frames instead of 12); Psybeam slightly off.

## Root cause
Stadium 1's threads start with `__osSetFpcCsr(0x01000C01)`: round toward zero. The port's `__osSetFpcCsr` was a stub, so game float math rounded to nearest and inexact results came out 1 ULP higher half the time; `ParticleMath_ApproachFloat` reported "reached" one frame early.

## Fix in stadion
`__osSetFpcCsr` sets the host rounding mode from the register's RM bits (`std::fesetround`, per host thread; each OSThread is one host thread, so the renderer keeps round-to-nearest). Traps and flush-to-zero are not mirrored.

## Zero / plethron
Present: `src/port/os_devices.cpp:369-383` (`kModes[4] = { FE_TONEAREST, FE_TOWARDZERO, FE_UPWARD, FE_DOWNWARD }; std::fesetround(kModes[csr & 3]);`). Zero's threads call `__osSetFpcCsr(0x01000F00)` (`game/src/dp_intro.c:221`, `game/src/game_state.c:820`, `game/src/display_object_textures.c:457`, `game/src/gb_player_launch.c:22,51`): RM bits 0 = round to nearest, so Zero's float results already match the host default. Keep the mechanism (it is correct either way); the comment at `os_devices.cpp:369-375` describes Stadium 1's 0x01000C01 and could mislead.

## How to detect
grep the decomp for `__osSetFpcCsr(` and read the low two bits; 1-ULP divergences against an emulator that flip threshold tests (approach/ease functions that stop one frame apart).
