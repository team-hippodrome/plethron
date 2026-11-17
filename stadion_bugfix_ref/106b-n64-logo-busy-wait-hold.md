# N64 logo held by a busy-wait on osGetTime: no frames presented for a second (frame gap at boot)

- **Category:** timing-pacing
- **Zero status:** check (measured under Xvfb with PORT_TRACE_PACING=1: longest present gap in the first 10 s was 70 ms, so a gap like stadion's 35-50 ms is likely there too; port Port_PacedLogoHold with its replay marker if Windows shows it)
- **stadion:** [PR #106](https://github.com/team-hippodrome/stadion/pull/106), commit(s) `153a418` (merge `7c408e8`)
- **stadion files:** `patches/game/20-fragment-n64_logo.patch` (n64_logo.c `N64Logo_Main`), `src/port/os_devices.cpp` (`Port_PacedLogoHold`, replay marker `# paced logo hold`)

## Symptom
At boot the logo stays up through a 1 s busy-wait; on Windows the measured longest frame gap was 35-50 ms and the
frame queue/audio did not advance smoothly during the hold.

## Root cause
`N64Logo_WaitForElapsed(start)` spins on `osGetTime()` without drawing. On N64 the VI keeps showing the last frame;
on the host the game thread spins while no frame is submitted, so presentation and pacing stall.

## Fix in stadion
When `Port_PacedLogoHold()` is on (default for new runs; recordings without the `# paced logo hold` line keep the
old timing), the hold draws the logo through the normal frame queue for 60 retraces:
```c
while (Port_ViTick() - holdStart < 60) { N64Logo_DrawFrame(0); BgStage_AdvanceFrame(); }
```

## Zero / plethron
Zero's `game/src/fragments/n64_logo/n64_logo.c:73-79` `N64Logo_HoldForOneSecond` busy-waits on `osGetTime()` and is
called at :157 after `N64Logo_ScanTransferPaks()`. plethron has no `Port_PacedLogoHold` (grep in `src/port` finds
only `Port_ViTick` at `os_devices.cpp:624`). Port it only if a pacing trace shows the gap on Zero; it also needs a
replay marker so existing Zero recordings keep their timing.

## How to detect
`PORT_TRACE_PACING=1` around boot (late retraces / present gaps during the logo). Grep the game for loops on
`osGetTime()`/`osGetCount()` that do not submit frames.
