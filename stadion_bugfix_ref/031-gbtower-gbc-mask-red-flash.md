# GB Tower: red-orange frames when a Game Boy Color game starts (screen mask ignored in GBC mode)

- **Category:** rendering
- **Zero status:** not applicable (plethron's `gb_core.c` has no GBC mode; colour carts run in SGB mode)
- **stadion:** [PR #31](https://github.com/team-hippodrome/stadion/pull/31), commit(s) `ab348d4`
- **stadion files:** `src/port/gb_core.c`, `PORTING_NOTES.md`

## Symptom
Starting Pokémon Yellow in the GB Tower showed ~30 red-orange frames with stray tiles before the copyright screen
(ares: white).

## Root cause
Yellow masks the screen (SGB MASK_EN) while it sends its border through VRAM and meanwhile converts its GBC palettes
through a still-null base pointer (WRAM 0xDEE6), reading colour 0x00FF from ROM 0x0000. Stadium's N64 emulator hides
this (drops BCPD 0xFF69 writes while MASK_EN is set for Pokémon carts, keeps the last picture while the mask is in
effect, unmask 4 frames late, initial CGB palettes white/white/black/black); the native core ignored the mask in GBC
mode.

## Fix in stadion
`gb_core.c`: drop masked BG palette writes, add `maskShown` / `unmaskDelay`, keep a `shown[]` picture for the
presentation, use Stadium's initial CGB palettes.

## Zero / plethron
`src/port/gb_core.c:15-17` states Zero's GB Player has no Game Boy Color emulation and CGB-flagged carts run in SGB
mode; `grep -n "cgb\|maskShown\|0x69" src/port/gb_core.c` finds nothing. MASK_EN in SGB mode is handled at
`gb_core.c:592-595` (`sgbMask`, frozen shades). Revisit only if GBC support is added to the GB Player.

## How to detect
Compare the GB screen frame by frame with ares during cart boot (border transfer phase); red/odd-colour frames or
stray tiles while MASK_EN is set point to mask handling.
