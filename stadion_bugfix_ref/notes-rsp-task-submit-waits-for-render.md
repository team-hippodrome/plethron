# Graphics tasks rendered asynchronously read display-list memory the scene had already freed

- **Category:** timing-pacing
- **Zero status:** already ported (`osSpTaskStartGo` waits for the window thread in the shared `src/port/os_devices.cpp`)
- **stadion:** no PR (Status "boots into the main menu", before PR #1; history squashed into `be110fe`). The RSP-task wake-up latency is PR #14, covered separately.
- **stadion files:** `src/port/os_devices.cpp`

## Symptom
Crashes / garbage when a scene ended: the renderer read display lists from freed memory.

## Root cause
On the N64 the scheduler only reuses a display-list buffer after the RDP signals done. The port first queued graphics tasks for the window thread and returned at once; scenes free their display-list data right after submitting their last frame, so the later asynchronous render read freed memory.

## Fix in stadion
`osSpTaskStartGo` waits until the window thread has rendered the task (lockstep with `gTasksProcessed`); `PORT_ASYNC_TASKS` keeps the old behaviour for debugging. JPEG tasks may run on the submitting thread when nothing else is queued.

## Zero / plethron
Already ported: `src/port/os_devices.cpp:1324-1370` (`osSpTaskStartGo`, `PORT_ASYNC_TASKS`, waits on `gTaskDoneCv` until `gTasksProcessed >= mine`).

## How to detect
`PORT_ASYNC_TASKS=1` with ASan reproduces use-after-free in the renderer; crashes in `interpreter.cpp` on scene changes.
