# Shifts by 32 or by a negative count: MIPS takes the count modulo 32, C on x86/arm64 does not (JPEG bit reader)

- **Category:** platform-build
- **Zero status:** already ported (`game/src/jpeg_decoder.c:196-199`, "from stadion")
- **stadion:** no PR (Status "boots into the main menu", before PR #1; history squashed into `be110fe`)
- **stadion files:** `game/src/jpeg_decoder.c`

## Symptom
The title screen's JPEG background (and every JPEG backdrop) decoded as garbage before the fix.

## Root cause
`Jpeg_ReadBits` shifts by `sJpegBitStreamBitIdx` and by `32 - len`, which can be 32 or more / negative. MIPS `sllv/srlv` use only the low 5 bits of the count; in C that is undefined behaviour, and x86 / arm64 compilers produce different results (x86 masks to 5 bits only for 32-bit ops; constant folding may give 0).

## Fix in stadion
```c
// PORT: the original relies on MIPS variable shifts taking the count modulo 32 (also for the negative count -len)
ret = sJpegBitStreamCurWord << (sJpegBitStreamBitIdx & 31);
temp = ret;
ret = temp >> ((32 - len) & 31);
```

## Zero / plethron
Already ported: `game/src/jpeg_decoder.c:196-199`, identical with the PORT comment "from stadion". Related class: integer division rounding toward zero (stadion PR #59, covered separately).

## How to detect
`-fsanitize=shift` (UBSan) on replays; grep `<< \(.*- ` / `>> \(32 -` in decoders and bit readers (GB emulator, GSM, packed animation streams).
