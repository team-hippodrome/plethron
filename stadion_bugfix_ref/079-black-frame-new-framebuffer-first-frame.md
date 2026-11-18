# One black frame after the title: a new frame buffer's first frame was rendered as an off-screen picture

- **Category:** rendering
- **Zero status:** already ported (plethron b1ea1c8: `DisplayTask_Submit` calls `Port_NoteFrameBuffer`)
- **stadion:** [PR #79](https://github.com/team-hippodrome/stadion/pull/79), commit(s) `62a7433` (squashed into `2db362c` locally)
- **stadion files:** `patches/game/02-core.patch` (src/dp_intro.c `DisplayTask_Submit`), `src/port/os_devices.cpp`, `PORTING_NOTES.md`

## Symptom
The first confirm on the title after boot showed one black frame between the title's white fade and the Game Pak
check. Later confirms did not (they reuse buffers already shown).

## Root cause
`GfxDpSetColorImage` in libultraship decides whether a screen-sized colour image is a frame buffer or an
off-screen picture (Gallery enlarge, Bonus stickers) by asking `Port_ViFrameBufferState(addr)` whether the VI has
already shown that address. A scene that allocates new frame buffers (here a hi-res 640-wide pair) draws its first
frame into a buffer that was never swapped, so that frame went to an off-screen texture; the swap then found no
GPU draw for the buffer (`stamp 0`) and presented its RAM, which was all zero.

## Fix in stadion
The display worker names the frame buffer of every task it submits before the RSP/RDP run it:
```c
void DisplayTask_Submit(UnkStruct80001380* arg0, struct UnkArray4* arg1, s32 arg2) {
    extern void Port_NoteFrameBuffer(const void* fb);
    if (arg1->unk_0C != NULL) {
        Port_NoteFrameBuffer(arg1->unk_0C->img_p);
    }
    ...
```
`Port_NoteFrameBuffer` (os_devices.cpp) inserts the address into the same set `osViSwapBuffer` fills.

## Zero / plethron
- Port side is present: `src/port/os_devices.cpp:421` defines `Port_NoteFrameBuffer`, declared in
  `src/port/StadionPort.h:47`; the classifier `Port_ViFrameBufferState` is at `os_devices.cpp:413` and is used by
  the pinned libultraship (`interpreter.cpp:4332` at 4148f93).
- Game side is missing: `game/src/dp_intro.c:70-74` `DisplayTask_Submit(SchedTask*, DisplayFrame* arg1, s32)` sets
  `data_ptr`/`data_size` and submits, with no call. `git grep Port_NoteFrameBuffer game/` finds nothing.
  `DisplayFrame` has the buffer at `arg1->framebuffer` (`game/src/dp_intro.h:17`, `struct GfxImage*`), so the fix is
  `if (arg1->framebuffer != NULL) Port_NoteFrameBuffer(arg1->framebuffer->img_p);` before submitting.
- Any Zero scene that switches to newly allocated frame buffers (hi-res/lo-res changes) can show one black or stale
  frame on its first appearance.

## Fixed in plethron (b1ea1c8)
`game/src/dp_intro.c` `DisplayTask_Submit` calls `Port_NoteFrameBuffer(arg1->framebuffer->img_p)` before submitting. `PORT_TRACE_VI=1` on the free_battle route: before, `swap ... stamp 0` (CPU-presented, empty) frames at 55, 1249 and 1251 (the title's START at 1200); after, only frame 1 (the boot clear).

## How to detect
`PORT_TRACE_VI=1` lists CPU-presented frames: a `swap ... stamp 0 ... CPU present nonzero 0` line at a scene change
is this bug. Step through a scene change frame by frame (screenshot every frame) on a fresh boot and compare with
a later visit.
