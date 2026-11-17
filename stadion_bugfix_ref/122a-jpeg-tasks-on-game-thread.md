# Pauses at screen changes: 300 small JPEG tasks each round-tripped through the window thread

- **Category:** performance
- **Zero status:** already ported (plethron's `osSpTaskStartGo` runs JPEG tasks on the submitting thread)
- **stadion:** [PR #122](https://github.com/team-hippodrome/stadion/pull/122), commit(s) `9438ec4` (merge `d045f77`)
- **stadion files:** `src/port/os_devices.cpp`, `src/port/main.cpp`, `src/port/Engine.h`

## Symptom
15-30 ms hitches at screen changes that decode JPEG backgrounds, with or without Smooth motion.

## Root cause
Each JPEG decode is ~300 RSP tasks submitted one after another. Every task went to the window thread and back
(~25 us) and cost a full window-loop pass including the Cocoa event pump.

## Fix in stadion
`osSpTaskStartGo` runs a JPEG task on the submitting thread when no other task is queued or in flight (order and
completion message unchanged); the main loop runs back-to-back tasks for up to 4 ms before pumping events.
`PORT_JPEG_ON_WINDOW_THREAD=1` restores the old path.

## Zero / plethron
`src/port/os_devices.cpp:1334-1335` (`PORT_JPEG_ON_WINDOW_THREAD`). Zero submits JPEG tasks from
`game/src/jpeg_stream.c:122`, which reaches this path.

## How to detect
`PORT_TRACE_PACING=1`: late retraces clustered at screen changes. Count tasks per frame; many tiny tasks that each
wait for another thread is the pattern.
