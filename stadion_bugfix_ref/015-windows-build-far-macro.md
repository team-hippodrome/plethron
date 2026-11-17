# Windows build broke: a local named `far` in the margin clear (windows.h macro)

- **Category:** platform-build
- **Zero status:** already ported (fork at 4148f93 uses `reach`)
- **stadion:** [PR #15](https://github.com/team-hippodrome/stadion/pull/15), commit(s) `4ad2f58`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`)

## Symptom
MSVC/MinGW builds failed to compile `interpreter.cpp` after PR #6.

## Root cause
`windows.h` defines `far` (and `near`) as empty macros, so `const int32_t right = ..., far = 4 * 4096;` expanded to
invalid code.

## Fix in stadion
Renamed the variable to `reach`:
```cpp
const int32_t right = (int32_t)mNativeDimensions.width * 4, reach = 4 * 4096; // not "far": windows.h macro
```

## Zero / plethron
`git -C /home/user/lus show 4148f93:src/fast/interpreter.cpp` line ~4479 has the `reach` version.

## How to detect
`grep -rnw "far\|near" src/port game/src --include=*.c --include=*.cpp` for identifiers (not comments); build on
Windows (plethron `tools/WINDOWS_VALIDATION.md`).
