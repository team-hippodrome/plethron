# ~0.35 s pause at boot and every scene change (window loop polled RSP tasks with a 1 ms sleep)

- **Category:** performance
- **Zero status:** already ported (`Port::WaitForRspTasks` in src/port)
- **stadion:** [PR #14](https://github.com/team-hippodrome/stadion/pull/14), commit(s) `8cd95ef`
- **stadion files:** `src/port/os_devices.cpp`, `src/port/main.cpp`, `src/port/Engine.h`

## Symptom
A visible ~0.35 s freeze at boot and at the title-to-menu change (first blamed on the save read).

## Root cause
Each scene decodes its backgrounds in ~300 JPEG RSP tasks (0.6 ms of work in total). Every task waited for the next
pass of the window loop, and that loop slept 1 ms between passes, so 300 round trips cost ~0.35 s.

## Fix in stadion
The window loop waits on a condition variable signalled when a task is queued (`Port::WaitForRspTasks(timeout)`,
`gTaskQueuedCv`) instead of sleeping, so it wakes as soon as the game posts a task. Pause ~25 ms afterwards.

## Zero / plethron
`src/port/os_devices.cpp:915` (`gTaskQueuedCv`), `:1636` (`Port::WaitForRspTasks`), used in `src/port/main.cpp:298`
and `:518`. Zero's JPEG/RSP tasks go through the same port code (`src/port/jpeg_task.cpp`).

## How to detect
`PORT_TRACE_PACING=1` sums RSP task time per retrace; a scene change with many tiny tasks and a long wall-clock gap
points to a polling loop. Search for `sleep_for` in loops that service game tasks.
