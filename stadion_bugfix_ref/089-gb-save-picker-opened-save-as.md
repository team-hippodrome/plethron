# Launcher: Transfer Pak "Change" save button opened Save As instead of picking an existing file

- **Category:** saves
- **Zero status:** already ported (plethron's launcher passes `false` for the GB save picker)
- **stadion:** [PR #89](https://github.com/team-hippodrome/stadion/pull/89), commit(s) `ffaefc2` (squashed into `2db362c` locally)
- **stadion files:** `src/port/launcher.cpp`

## Symptom
Choosing a Game Boy battery save showed a Save As panel; the chosen file was treated as new, and Game Pak Check
said "Saved file not found".

## Root cause
`Pick(&g.save, true, kGbSaveFilter, 2, save)`: the second argument selects the save dialog.

## Fix in stadion
One line: `Pick(&g.save, false, kGbSaveFilter, 2, save);`.

## Zero / plethron
`src/port/launcher.cpp:1189`: `Pick(&g.save, false, kGbSaveFilter, 2, save);`.

## How to detect
Click every file button in the launcher once on each OS; any picker for an input file must be an Open dialog.
`grep -n "Pick(" src/port/launcher.cpp` and check the boolean against the intent.
