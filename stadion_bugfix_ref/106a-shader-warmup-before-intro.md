# Startup stutter: shader warm-up still running while the intro played; now finished before the intro

- **Category:** performance
- **Zero status:** already ported (plethron main.cpp runs `Port::RunShaderWarmup()` before `Main()`)
- **stadion:** [PR #106](https://github.com/team-hippodrome/stadion/pull/106), commit(s) `153a418` (merge `7c408e8`)
- **stadion files:** `src/port/shader_warmup.cpp`, `src/port/main.cpp`

## Symptom
Frame gaps of 35-50 ms in the N64 logo/intro on Windows, especially on a fresh install.

## Root cause
`StartShaderWarmup` only queued the program list; the game started at once and its first frames competed with
(or waited for) programs still being compiled.

## Fix in stadion
`RunShaderWarmup()` queues the list and pumps the window with an ImGui progress bar ("Preparing shaders") over a
black screen until `ShaderWarmupProgress() >= 1`, then the intro starts. Returns false if the window was closed.

## Zero / plethron
`src/port/shader_warmup.cpp:159` `bool RunShaderWarmup()`, called at `src/port/main.cpp:500` before `Main()` (:505).
Whether the list covers Zero's programs is case 080.

## How to detect
`PORT_TRACE_SHADERS=1`: `[shader] startup warm-up complete; starting intro` must come before the first game frame,
and no `built in X ms` lines should follow during the intro. stadion's `tools/windows_intro_regression.py` reports
the longest frame gap of the logo.
