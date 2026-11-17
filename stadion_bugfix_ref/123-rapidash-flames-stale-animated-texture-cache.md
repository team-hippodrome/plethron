# Rental screen: Rapidash's flames used Ponyta's texture offsets (stale cache keyed by host address)

- **Category:** rendering
- **Zero status:** not applicable (Zero's model-callback table conversions are rebuilt on every call, no cache)
- **stadion:** [PR #123](https://github.com/team-hippodrome/stadion/pull/123), commit(s) `19d0a12` (merge `c2e22ce`)
- **stadion files:** `patches/game/20-fragment-particle_data_library.patch`, `PORTING_NOTES.md`

## Symptom
After viewing Ponyta in the rental list, Rapidash's flames were flat yellow with red/white patches changing with
the pose.

## Root cause
The particle data library converted a model part's animated-texture record and address lists once and cached them
by host address. The rental screen loads each Pokemon's model into the same block, so Rapidash's record hit
Ponyta's cache entry and sampled Ponyta's texture offsets (in Rapidash's blob: body and eye textures).

## Fix in stadion
Each cache entry keeps the raw bytes it was converted from and converts again when they differ; a full table
reuses entries in turn instead of allocating every call.

## Zero / plethron
Zero's equivalents are `ModelCallback_HostAddresses` and `ModelCallback_HostScrollStrip`
(`game/src/fragments/model_callbacks/model_callbacks.c:39-71`; the comment above at :23 says "The copies are rebuilt
on every call"). The scroll strip used by Ponyta/Rapidash/Moltres (`model_callbacks_7.c`) reads through
`HostScrollStrip`, which re-reads the source each time. No address-keyed cache exists in `model_callbacks/` or
`particle_data/` (grep `static.*cache`). Related plethron fix for translated DLs: `0f590be`.

## How to detect
Any host-side cache keyed by an address inside a buffer the game reuses (model blocks, archives). View two
different models in the same slot in a row; check that the second looks right.
