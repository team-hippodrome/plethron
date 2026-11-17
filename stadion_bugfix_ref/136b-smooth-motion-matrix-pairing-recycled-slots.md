# Smooth motion slid recycled effects (sleep Zs, flames, sparkles) towards other objects' places

- **Category:** rendering
- **Zero status:** already ported (plethron `src/port/interp.cpp` is identical to stadion's current one)
- **stadion:** [PR #136](https://github.com/team-hippodrome/stadion/pull/136), commit(s) `a8eecf2`
- **stadion files:** `src/port/interp.cpp`

## Symptom
In in-between pictures Fire Blast's burst collapsed, sleep Zs jumped, Koga's gym sparkles vanished.

## Root cause
Matrices of consecutive frames were paired as "n-th matrix of (call site, node)". Effects that recycle slots (oldest
goes, new one comes, the rest move up one place) shift that list every frame, so the n-th matrix belonged to another
object: ~11 mis-pairings per frame in the cup-clear ceremony, 2,000-3,400 per battle.

## Fix in stadion
`MatchGroup` keeps n-th pairing only when every matrix's partner is also its nearest one; otherwise each takes its
nearest unambiguous partner, and anything ambiguous is drawn as built. `PORT_INTERP_MATCH=index` restores the old rule.

## Zero / plethron
Present: `src/port/interp.cpp:335-350` (`MatchGroup`, `PORT_INTERP_MATCH=index`). Zero shares the battle particle code
(sleep Zs, flames), so it benefits; nothing to do.

## How to detect
`PORT_INTERP_LOG=2` logs pairing stats; compare in-between pictures against their neighbours (`tools/interp_check/`).
