# Launcher Save editor button did nothing in packaged apps (missing tools dir, Finder PATH)

- **Category:** platform-build
- **Zero status:** not applicable (plethron's launcher uses the built-in C++ save editor, no Python or tools folder)
- **stadion:** [PR #122](https://github.com/team-hippodrome/stadion/pull/122), commit(s) `d97efa0` (merge `d045f77`); superseded by stadion #130
- **stadion files:** `src/port/launcher.cpp`, `src/port/save_editor_mac.mm`, `tools/make_macos_bundle.py`, `CMakeLists.txt`

## Symptom
The Debug "Save editor" button opened a browser on an editor that failed to load.

## Root cause
It looked for `tools/save_editor` next to the program (absent in a bundle) and started `python3` with Finder's
minimal PATH, then opened the browser regardless.

## Fix in stadion
Bundle the editor in Contents/Resources, look for python3 in Homebrew/python.org locations, open it in a WKWebView
window on macOS, report a missing save. Later (#130) replaced by an editor built into the game.

## Zero / plethron
`src/port/launcher.cpp:22` includes `save_editor/save_editor_ui.h` and `:581-591` (`gSaveEditor`, `OpenSaveEditor`)
draw the built-in editor (`src/port/save_editor/`). There is no `save_editor_mac.mm` and no Python launch.

## How to detect
Test launcher buttons from a packaged build started from Finder/Explorer, not from a shell in the source tree.
