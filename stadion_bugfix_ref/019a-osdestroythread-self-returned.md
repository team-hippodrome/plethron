# Quitting the Game Boy emulator closed the game: osDestroyThread(NULL) returned

- **Category:** game-logic
- **Zero status:** already ported (`src/port/os_thread.cpp:247-254`; Zero calls `osDestroyThread(NULL)` in gb_emu_draw.c:330)
- **stadion:** [PR #19](https://github.com/team-hippodrome/stadion/pull/19), commit(s) `09b97fc`
- **stadion files:** `src/port/os_thread.cpp`

## Symptom
GB Tower menu "Quit Game" closed the whole game: a crash (fault at 0x5e00), not an exit.

## Root cause
When the emulator shuts down, the game sends NULL to the Game Boy Pak service thread, which calls
`osDestroyThread(NULL)` on itself. On the N64 that call never returns. The port's libultra emulation marked the
thread dead and returned, so the thread continued with the NULL message and crashed.

## Fix in stadion
In `osDestroyThread`, a thread destroying itself never returns: after giving up the run token it blocks forever on its
condition variable (a dead thread is never scheduled again):
```cpp
if (th == tlsSelf) {
    for (;;) {
        th->cv.wait(lk);
    }
}
```

## Zero / plethron
Present at `src/port/os_thread.cpp:247-254`. Zero's GB Player hits the same path:
`game/src/fragments/audio_and_gb_emulator/gb_emu_draw.c:330` calls `osDestroyThread(NULL)`.

## How to detect
Any libultra call that "never returns" on the N64 (`osDestroyThread(NULL)`, `osStopThread(NULL)` on self, fatal
loops) must not return in the host emulation. Run a GB Player session to its Quit and check the game keeps going.
