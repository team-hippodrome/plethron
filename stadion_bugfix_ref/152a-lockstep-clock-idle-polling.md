# Lockstep clock polled for "game idle" every 20 us (~28k wake-ups/s, more kernel than game time)

- **Category:** performance
- **Zero status:** needed (plethron's `LockstepClock` still sleeps 20 us in a polling loop)
- **stadion:** [PR #152](https://github.com/team-hippodrome/stadion/pull/152), commit(s) `2037ff5`
- **stadion files:** `src/port/os_devices.cpp`, `src/port/os_thread.cpp`

## Symptom
High CPU and system time in every mode (lockstep is the default): battle replay at max speed 3.97 s sys vs 3.57 s
user, 896k context switches.

## Root cause
Before each retrace/timer event the clock waits until all game threads are blocked and all RSP tasks are done, and
checked that by `sleep_for(20us)` in a loop.

## Fix in stadion
Condition variable + epoch counter (`gIdleLock`, `gIdleCv`, `gIdleEpoch`, `NotifyMaybeIdle()`), bumped in
`Reschedule` when nothing is runnable and at every task completion (CPU task thread, `osSpTaskStartGo` sync path,
`Port::PumpRspTasks`). The epoch is read before the idle checks; 1 ms timeout as a safety net. Same idle condition,
so runs stay deterministic. Result: 896k → 126k context switches, 34.6 G → 21.6 G instructions.
```cpp
for (;;) {
    uint64_t epoch; { std::lock_guard<std::mutex> g(gIdleLock); epoch = gIdleEpoch; }
    if (TasksDrained() && port_ThreadsIdle() && TasksDrained()) break;
    if (gStop) return;
    { std::unique_lock<std::mutex> g(gIdleLock);
      gIdleCv.wait_for(g, std::chrono::milliseconds(1), [&] { return gIdleEpoch != epoch || gStop; }); }
```

## Zero / plethron
Missing: `src/port/os_devices.cpp:1052-1054`
(`while (!(TasksDrained() && port_ThreadsIdle() && TasksDrained())) { ... sleep_for(std::chrono::microseconds(20));`);
`src/port/os_thread.cpp:100-110` `Reschedule` has no notify on the `best == nullptr` branch; no `NotifyMaybeIdle`
anywhere. `os_thread.cpp` is byte-identical to stadion 766c25d, so stadion's diff applies directly; `os_devices.cpp`
differs (plethron edits), so port by hand (also the three task-completion sites).

## How to detect
`grep -n "microseconds(20)" src/port/os_devices.cpp`; `/usr/bin/time -l` (macOS) or `perf stat -e context-switches`
on a headless replay.
