# stadium_save_cli on Windows: non-ASCII arguments (♂, é) arrived as "?"

- **Category:** platform-build
- **Zero status:** already ported (plethron `src/port/save_editor/cli_main.cpp` reads the wide command line)
- **stadion:** [PR #130](https://github.com/team-hippodrome/stadion/pull/130), commit(s) `754f70c`
- **stadion files:** `src/port/save_editor/cli_main.cpp`

## Symptom
Two save-editor parity tests failed under Wine: names with ♂ / é reached the Windows CLI as `?`.

## Root cause
Windows hands `main(int, char**)` its arguments in the ANSI code page, which cannot hold those characters.

## Fix in stadion
On `_WIN32`, rebuild the arguments from `CommandLineToArgvW(GetCommandLineW())` converted with
`WideCharToMultiByte(CP_UTF8, ...)`.

## Zero / plethron
Present: `src/port/save_editor/cli_main.cpp:52-56` (`CommandLineToArgvW`, `WideCharToMultiByte(CP_UTF8, ...)`).

## How to detect
`grep -rn "int main(int argc" src/` for Windows entry points that use `argv` directly with user text or paths; run
with a non-ASCII argument under Wine or Windows.
