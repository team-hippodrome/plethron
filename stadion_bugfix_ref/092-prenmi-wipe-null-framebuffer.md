# Reset (PRE-NMI) wipe used libultraship's null current-framebuffer stub and detached reset globals

- **Category:** abi-64bit
- **Zero status:** not applicable (Zero's reset wipe uses its own typed `shownFrame.framebuffer`, not `osViGetCurrentFramebuffer`)
- **stadion:** [PR #92](https://github.com/team-hippodrome/stadion/pull/92), commit(s) `23cc9a0` (squashed into `2db362c` locally)
- **stadion files:** `patches/game/02-core.patch` (src/reset.c), `src/port/os_devices.cpp`, `src/port/sdk/port_compat.h`

## Symptom
After pressing reset, the fade wipe cleared one line in 320 mode and the display worker stayed blocked; the wipe
helpers did not run as on N64.

## Root cause
`osViGetCurrentFramebuffer` was libultraship's stub returning NULL. The reset worker structure stored the
framebuffer in an `s32` (`UnkStruct4.unk_08`), and the wipe helpers used `extern gSoftResetLineNum/Red/Green/Blue`
symbols that on N64 alias fields of `D_800A6D20` but on the host are separate variables.

## Fix in stadion
`port_osViGetCurrentFramebuffer` returns the last presented buffer (`gViCurrent`, set in `TryPresentCpuFrame`) and
is macro-mapped in `port_compat.h`; `unk_08` became `u16*`; reset.c reads `D_800A6D20.gSoftReset*` fields directly.

## Zero / plethron
The port wrapper is inherited (`src/port/sdk/port_compat.h:141,148`), but Zero's game never calls
`osViGetCurrentFramebuffer` (grep in `game/src` finds nothing) and has no reset.c. Its wipe is
`Display_ClearForReset` (`game/src/dp_intro.c:174`), called from `DisplayWorker_ThreadMain` when
`D_800A9710.preNmiFrames > 0` (:228); it reads `D_800870D0.shownFrame.framebuffer` (a typed `struct GfxImage*`,
`dp_intro.h:17`) and falls back to `osViBlack(1)`.

## How to detect
Grep the game for `extern` globals that are N64 aliases of struct fields (same address as a member) and for libultra
calls that libultraship stubs (`osViGetCurrentFramebuffer`, `osViGetNextFramebuffer`). Port side: post event 14
(`port_PostEvent(14)`) after the title and watch the wipe.
