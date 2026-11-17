# GB Tower: cartridge detection walked backwards from one title array into another (host linkers reorder data)

- **Category:** platform-build
- **Zero status:** not applicable (Zero's `GbPak_IdentifyCartridge` walks forward inside one 96-byte table)
- **stadion:** [PR #90](https://github.com/team-hippodrome/stadion/pull/90), commit(s) `34201be` (squashed into `2db362c` locally)
- **stadion files:** `patches/game/20-fragment-gb_tower_emulator.patch` (gb_tower_emulator_7F9A0.c `GbPak_DetectSupportedCartridgeVariant`)

## Symptom
On Windows, supported cartridges were not recognised by GB Tower.

## Root cause
`GbPak_DetectSupportedCartridgeVariant` started at the Yellow title (`D_8120DD70`) and stepped `var_s1 -= 0x10` to
reach Red/Green/Blue, which on N64 sit just before it inside `D_8120DD2C`. Separate C arrays have no guaranteed
order or adjacency on the host; the Windows linker placed them elsewhere.

## Fix in stadion
Address each entry explicitly:
`var_s1 = (i == 4) ? D_8120DD70 : &D_8120DD2C[0x14 + (i - 1) * 0x10];`

## Zero / plethron
Zero's counterpart `GbPak_IdentifyCartridge` (`game/src/fragments/audio_and_gb_emulator/gb_emu_draw.c:1135`) starts
at `D_81122854` and advances `entry += 0x18` four times. `D_81122854` is one 96-byte object
(`src/port/generated/zero_lift_data.c:648`, `ZLIFT_STORAGE(D_81122854, 96)` = 4 x 0x18), so the walk stays inside it.

## How to detect
Pointer arithmetic that leaves one array to reach a neighbouring symbol: `extern u8 X[]` followed by `p -= N` or
`p += N` beyond its size. Compare each array's size in the lifted data with the loop's span.
