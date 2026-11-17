# Windows: crashes after startup left no log (port's exception filter replaced libultraship's)

- **Category:** platform-build
- **Zero status:** already ported (inherited port layer)
- **stadion:** [PR #42](https://github.com/team-hippodrome/stadion/pull/42), commit(s) `6f0467e`
- **stadion files:** `src/port/main.cpp`

## Symptom
On the affected Windows machine, after the window opened the game died with no log entry and no message box. A failed D3D/DXGI call or an uncaught C++ exception also ended the process silently.

## Root cause
After `Port::Init`, `main.cpp` installed the port's own `SetUnhandledExceptionFilter`, replacing libultraship's. The port's handler wrote only to stderr, which a GUI (windowed-subsystem) build has nowhere to send, then exited. No `std::terminate` handler logged uncaught exceptions.

## Fix in stadion
The port's filter keeps the previous filter and calls it after its own report, so libultraship's crash log still gets written; a `std::terminate` handler logs uncaught exceptions.

## Zero / plethron
Present: `src/port/main.cpp:171-180` (`LPTOP_LEVEL_EXCEPTION_FILTER previous = SetUnhandledExceptionFilter(CrashHandler); ... sShipCrashHandler = previous; std::set_terminate(TerminateHandler);`).

## How to detect
Force a crash in a Windows GUI build (e.g. a null write behind an env var) and check the log file; any `SetUnhandledExceptionFilter`/`signal` installation that drops the previous handler.
