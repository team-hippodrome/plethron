# Byte-order helpers were static copies per file: other files hit "call to undeclared function"

- **Category:** platform-build
- **Zero status:** already ported (`game/include/port_endian.h`, duplicate check in `tools/check_port_decls.py`)
- **stadion:** [PR #148](https://github.com/team-hippodrome/stadion/pull/148), commit(s) `766c25d`
- **stadion files:** `game/include/port_endian.h`, `game/include/macros.h`, `tools/check_port_decls.py`, 9+ game files

## Symptom
`demo_scenes_165640.c`: `call to undeclared function 'Port_SwapWords'` although it includes `StadionPort.h`.
(Before `-Werror=implicit-function-declaration`, C would have assumed an `int` return.)

## Root cause
`Port_SwapWords` was `static` in `jpeg_stream.c` and `battle_engine_move_animation.c`; several files had their own
`Port_MiniFxBE32`, `Port_MatBE32/16`, `PortA_BE32/16`, `Save_PortSwap16/32` and open-coded `(p[0]<<24)|...` reads.

## Fix in stadion
One header, `port_endian.h` (via `macros.h`): `Port_ReadBE32/16`, `Port_SwapWords/Halves` (memcpy, any alignment),
`Port_Swap32/16`, `PORT_BE32` (no-ops on big-endian hosts). `check_port_decls.py` fails when a `Port*_` function is
defined in more than one file.

## Zero / plethron
Present: `game/include/port_endian.h`; `CMakeLists.txt:278` `-Werror=implicit-function-declaration`;
`python3 -I tools/check_port_decls.py` → "OK: no local Port_* declarations or duplicate Port* helpers in game/".

## How to detect
`tools/check_port_decls.py`; `grep -rnE "static .*Port_(Swap|Read)" game/src`.
