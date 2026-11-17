# Blank screen between scenes: the one-line frame buffer must be written in N64 byte order (dark green instead of black)

- **Category:** endianness
- **Zero status:** needed (Zero's `Display_ClearFramebufferLine` writes host order)
- **stadion:** no PR (third ares round, before PR #1; history squashed into `be110fe`)
- **stadion files:** `game/src/dp_intro.c`, `src/port/os_devices.cpp`, `src/port/stubs.c`

## Symptom
Between scenes the game shows a one-line 640-pixel buffer with `osViRepeatLine(1)`. First the port showed red/purple blocks at every scene change (osViRepeatLine was a no-op stub, so 320x240 pixels were read from unrelated memory). After implementing repeat-line mode, the "black" blank screen came out dark green (0,32,0).

## Root cause
1. `osViRepeatLine` was a stub (port side, fixed in `os_devices.cpp`; `osViSetMode` ends repeat-line mode).
2. `PresentCpuFrame` presents CPU frame buffers as RGBA5551 in N64 (big-endian) byte order, like RDRAM. `Display_ClearFramebufferLine(color)` stored the u16 natively, so black-with-coverage `0x0001` was read back as `0x0100` (green 4/31).

## Fix in stadion
`dp_intro.c:228-245`: write and read the line byte-swapped:
```c
*(buf)++ = (u16)((color << 8) | (color >> 8)); // PORT: frame buffers hold big-endian RGBA5551
...
return (u16)((*ptr << 8) | (*ptr >> 8)); // Display_GetFramebufferClearColor
```

## Zero / plethron
- Port side already inherited: `src/port/os_devices.cpp:445-457` (`port_osViRepeatLine`), `src/port/stubs.c:103-104`, `PresentCpuFrame` (`os_devices.cpp:1422-1468`; comment at `:1407`) documents "RGBA5551, N64 byte order".
- **Needed:** `game/src/dp_intro.c:270-277` `Display_ClearFramebufferLine` writes `*(buf)++ = color;` (host order) and `dp_intro.c:280-283` `Display_GetFramebufferClearColor` returns `*ptr` unswapped. Callers: `dp_intro.c:255` (`Display_ClearFramebufferLine(1)`), `stage_loader.c:374, 472`, and `stage_loader.c:312` reads the colour back. Swap both functions as stadion did (keep the pair consistent, since `stage_loader.c:312` saves and later restores the colour).

## How to detect
`PORT_TRACE_VI=1` lists CPU-presented frames and their first non-zero pixels; a scene change that should be black but logs `0100`-style pixels, or a green/coloured flash between screens, is this. Grep game code for CPU writes into `GfxImage->img_p` / frame buffers (`u16*` loops) that do not swap.
