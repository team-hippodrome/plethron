# Cup grid showed the wrong trainer portrait (big-endian u16 read by its low byte)

- **Category:** endianness
- **Zero status:** not applicable (Zero picks the VS portrait from a host-written u8 `trainerIndex`, not a ROM trainer record)
- **stadion:** [PR #12](https://github.com/team-hippodrome/stadion/pull/12), commit(s) `7cbfc13` (game part: michiiik/pokestadium#297)
- **stadion files:** `game/src/fragments/battle_prep/battle_prep_397B10.c` (today; was a submodule change)

## Symptom
On the Poké Cup grid the first opponent (a Biker) showed Bruno's face.

## Root cause
The portrait file index is the high byte of a big-endian u16 field (`unk_038`) of the trainer record, a file of a
`BinArchive` loaded raw from ROM. The port read the u16 natively and took `>> 8 & 0xFF`, i.e. the wrong byte on a
little-endian host.

## Fix in stadion
```c
tmp = (Port_Swap16(temp_s5[gSessionContext.opponentHistory[0][i]].unk_038) >> 8) & 0xFF;
```
Same in the mode-7 badge loop (`battle_prep_397B10.c:1713`, `:1726`).

## Zero / plethron
Zero has no cup grid built from trainer records. Its VS screen takes the portrait file as
`BinArchive_GetFile(D_8220325C, team->rosterInfo[0]->trainerIndex)` (`game/src/fragments/vs_scene/vs_scene.c:187`),
and `trainerIndex` is a u8 written by C code (`game/src/fragments/nintendo_cup_teams/nintendo_cup_teams.c:34,92-147`,
`free_battle_setup_2.c:43`). No `Port_Swap16`-style read is missing there.

## How to detect
Any `(x >> 8) & 0xFF` or `& 0xFF` on a field of a struct pointing into a `BinArchive_GetFile` result is suspect:
`grep -rn ">> 8) & 0xFF" game/src`. Compare portraits against ares/emulator screenshots.
