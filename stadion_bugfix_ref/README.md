# stadion bug-fix reference

stadion (team-hippodrome/stadion, the Stadium 1 PC port) fixed many bugs that Stadium Zero's port can hit too, since both use the same libultraship approach and share a lot of engine code. This folder has one file per stadion bug fix. Each file gives the symptom, root cause, the stadion PR and files, the fix, whether plethron still needs it (with file and line evidence from plethron's code), and how to find the same bug class elsewhere.

Checked against plethron at `6ecd2d8` (head of PR #27), libultraship pin `4148f93`, stadion `main` at `704db9d` (PR #157).

## Read this first: what plethron already has

- **`src/port/` came from stadion at PR #148** (2026-11-15). About 77 port files are byte-identical. That means port-layer fixes merged up to #148 are normally already in plethron. Port fixes merged after that (#152, #157) are not.
- **libultraship:** plethron pins `4148f93` and stadion pins `8556b99`. Every renderer fix up to the pin is inherited, including everything that used to live in stadion's `patches/libultraship.patch`. The 5 fork commits after the pin are cases `lus-*`, and they come in with one submodule bump.
- **`game/` is the Zero decomp**, not stadion's. stadion's game-side fixes are never inherited and have to be redone in Zero's code. Several "needed" cases are this split: **the port half of the fix is already in `src/port`, but Zero's game code never calls it** (075 controllers, 079 black frame, 116/120a widescreen helpers, 137a volume sliders, notes-forget-render-targets).
- Cases about modes Zero doesn't have (Gym Leader Castle, Gallery, Victory Palace, minigames, Trade Machine, GBC mode) are marked *not applicable*, so nobody has to check them again.

## Statuses

- **needed**: plethron still has the bug or lacks the fix; the case names the exact place.
- **check**: could not be decided from the code alone; the case says what to look at.
- **already ported**: present in plethron (inherited or redone), with the location.
- **not applicable**: the code path or mode doesn't exist in Zero, or Zero's code is already shaped differently.

## Suggested assignment of the "needed" cases

1. **Wrong behaviour or crash in Zero code (game-side):** 064 (いちらん filter count doubled), 070 (Lab box checks with the N64 stride), 060 (raw animation `endFrame` reads in battle), 055 (Hyper Beam / Ice Beam strands, uninitialised phase), notes-narrow-fields-through-wide-unions (animation frame counter, GB APU phase), notes-vi-blank-line-n64-byte-order, notes-forget-render-targets-on-pool-reuse, 075 (controllers 2-4), 079 (black frame after the title).
2. **Settings that do nothing:** 137a (the Music / Sound effects / Voices sliders have no game hook in Zero).
3. **Widescreen game hooks:** 116 (Surf water panel), 120a (Hypnosis-style effect grids). See also stadion PR #119 (battle HUD anchors), which is a feature rather than a bug fix and so has no case here, though plethron lacks it too.
4. **Windows paths:** 103 (`gb_save_convert.cpp` opens with narrow `fopen`).
5. **One libultraship bump** (`4148f93` to `8556b99`): lus-bc50ce8, lus-5a3431c, lus-003614f, lus-a17638c (and lus-89d2976 comes with it).
6. **Port-layer performance after #148:** 152a, 152b, 157 (port them together; 157 builds on 152b).

## Adding a case

Name it `NNN-slug.md` with the stadion PR number (`NNNa`/`NNNb` for a PR with several fixes), `lus-<sha7>-slug.md` for a libultraship-only fix, or `notes-slug.md` for a fix known only from stadion's `docs/archive/PORTING_NOTES.md`. Copy the headings of an existing case, then rebuild the tables below with:

```
python3 tools/stadion_bugfix_index.py
```

When a "needed" case gets fixed, change its status line to `already ported (plethron <sha>)` and rerun the script.

## Index

<!-- BEGIN INDEX -->
147 cases: 20 needed, 11 check, 86 already ported, 30 not applicable.

### Needed in plethron

| Case | Category | Notes |
|---|---|---|
| [Hyper Beam / Ice Beam strands lost their wave: a never-written local folded to 0 by clang](055-beam-strands-uninitialised-stack-phase.md) | game-logic | same function, same unset `spA8` |
| [Raw big-endian animation headers read natively: endFrame 256x too large (Dream Eater's sparkles missing)](060-raw-animation-endframe-reads.md) | endianness | four raw `endFrame` reads remain in Zero's battle code |
| [Lab list filters: PointerList_Filter counted 4-byte pointers (count doubled, garbage rows, crash)](064-pointerlist-filter-count-pointer-size.md) | abi-64bit | same `>> 2` in Zero's table_view.c |
| [Lab PC boxes read through an `s32*` view of a pointer list / N64 row stride (Check BOX crash, icons shifted)](070-lab-pc-box-pointer-list-read-through-s32.md) | abi-64bit | Zero's `LabBoxList_ApplyChecks` walks the box rows with the N64 stride and offset |
| [Only controller 1 was presented to the game (every multiplayer choice greyed out)](075-controllers-2-4-presented-to-game.md) | input | port side inherited, but Zero's `Cont_SetupControllers` does not use it |
| [One black frame after the title: a new frame buffer's first frame was rendered as an off-screen picture](079-black-frame-new-framebuffer-first-frame.md) | rendering | the port half is inherited, but Zero's `DisplayTask_Submit` never calls `Port_NoteFrameBuffer` |
| [Windows: settings and save files under non-ASCII paths (narrow fopen/ifstream on UTF-8 paths)](103-unicode-config-and-save-paths.md) | saves | save/config paths are fixed, but plethron's own `gb_save_convert.cpp` still opens its store with plain `fopen` |
| [Widescreen: Surf's camera-facing water panel kept its 4:3 width (visible edges in 16:9)](116-surf-water-panel-widescreen.md) | rendering | helper `Port_WidenViewSpan` is inherited but Zero's `SurfTerrainGrid_BuildCameraBounds` does not call it |
| [Widescreen: Hypnosis' swirl (camera-facing effect grids) stopped short of the right edge](120a-hypnosis-effect-grid-widescreen.md) | rendering | helper `Port_WidenEffectGridX` is inherited; Zero's three grid inits do not call it |
| [Music / Sound effects sliders followed slider^4 (50 % = -24 dB, 25 % silent)](137a-music-effects-volume-slider-curve.md) | audio | the curve fix is present, but Zero's game never applies the Music/Effects/Voices gains, so those sliders do nothing |
| [Lockstep clock polled for "game idle" every 20 us (~28k wake-ups/s, more kernel than game time)](152a-lockstep-clock-idle-polling.md) | performance | plethron's `LockstepClock` still sleeps 20 us in a polling loop |
| [Window loop pumped OS events and polled pads on every ~1 ms pass (85 % of the window thread's time)](152b-event-pump-every-loop-pass.md) | performance | plethron `PumpFrame` pumps unconditionally; see also `157-...` |
| [Event pump's 4 ms floor still cost twice the game's drawing; replaced by 33 ms floor + mid-frame pump](157-event-pump-4ms-floor.md) | performance | depends on `152b-...`, also missing |
| [Each render-to-texture readback waited for the GPU on its own (up to 8 ms hitches at scene changes)](lus-003614f-rtt-readback-batch.md) | performance | libultraship bump; Zero's VS screen and portraits use off-screen targets |
| [Texture cache key hashed every imported texture one byte at a time (FNV-1a, ~19 us per 4 KB)](lus-5a3431c-texture-hash-8-bytes.md) | performance | libultraship bump; pin 4148f93 has the byte loop |
| [Metal screenshots showed the previous capture point's frame (or black), not the frame just drawn](lus-a17638c-metal-capture-stale-frame.md) | rendering | libultraship bump; plethron's replay/screen-diff tools rely on `PORT_SHOT_AT` captures |
| [Scrolled wrapping textures squashed for a few frames (mipmap-style tile trim applied to a wrapping axis)](lus-bc50ce8-wrap-axis-pyramid-trim.md) | rendering | libultraship bump; plethron's pin 4148f93 predates it, and Zero draws the same 32x64 sleep-Z sheet |
| [Memory reused after a render target still served stale GPU pixels (black HUD labels): tell the renderer when a pool block is handed out again](notes-forget-render-targets-on-pool-reuse.md) | rendering | the hook exists in libultraship, Zero's allocator never calls it |
| [Unions that overlay a narrow field on a wider integer read the wrong half on a little-endian host (animation frame counter, GB APU phase)](notes-narrow-fields-through-wide-unions.md) | endianness | Zero's `AnimFixedFrame` and `GbApuChannelState.phase` still use the N64 member order |
| [Blank screen between scenes: the one-line frame buffer must be written in N64 byte order (dark green instead of black)](notes-vi-blank-line-n64-byte-order.md) | endianness | Zero's `Display_ClearFramebufferLine` writes host order |

### Check (could not be decided from the code alone)

| Case | Category | Notes |
|---|---|---|
| [Pikachu Surf reward crash: animation in a data fragment read as native (not recognised as raw ROM data)](067a-pikachu-reward-fragment-animation-raw.md) | endianness | Zero's raw test does not cover segment/fragment-mapped data |
| [Lab PC list: box picker grid, column header page and "Group by TYPE" read through N64-offset views](068b-lab-pc-list-grids-n64-offset-views.md) | abi-64bit | Zero's いちらん screens differ; one equivalent bug found, see 070 |
| [Non-void game functions that fall off the end: the host returns garbage where the N64 left v0 (VS Mewtwo intro skipped)](074-functions-falling-off-the-end-v0.md) | game-logic | 34 such functions in Zero; most results unused, three audio/GB ones need a look |
| [First Poke Ball throw stutter: fogged translucent shader programs were missing from the warm-up list](080-shader-warmup-fogged-translucent-programs.md) | performance | the list is inherited byte-identical, but it holds Stadium 1's programs; Zero's own scenes need their own recorded list |
| [N64 logo held by a busy-wait on osGetTime: no frames presented for a second (frame gap at boot)](106b-n64-logo-busy-wait-hold.md) | timing-pacing | Zero has the same busy-wait, `N64Logo_HoldForOneSecond`; decide whether the gap shows on Zero's targets |
| [First-entry save stall: every FlashRAM page write rewrote the save file (with backups)](106c-flash-save-per-bank-write-stall.md) | performance | Zero saves to SRAM; plethron writes the whole .sra with a backup snapshot on every changed DMA write |
| [Vulkan stutter at first battle/scene: pipeline states built at first draw (warm-up list)](108-vulkan-pipeline-warmup-first-battle.md) | performance | the list file is inherited byte-identical; its entries come from Stadium 1 recordings |
| [Android APK build failed: NDK's -Werror=format-security not undone by the game's -Wno-error](153-android-format-security-werror.md) | platform-build | flag missing in plethron, but no triggering call found in Zero's game code today |
| [Texture rectangles copying the shown frame buffer onto itself painted stale RDRAM (black/noise on OpenGL)](lus-89d2976-framebuffer-self-copy-texrect.md) | rendering | comes with the libultraship bump; the known trigger is Stadium 1's Gallery, absent in Zero |
| [Code that indexes past one global into the next (N64 link order) reads or writes unrelated host memory](notes-adjacent-globals-out-of-bounds.md) | abi-64bit | Zero's data is lifted per symbol, sized to the gap to the next N64 symbol; reads before a symbol, e.g. `gMoveData[-1]`, are not covered |
| [Buffers the N64 overran harmlessly crash on the host (unterminated GB names, char[1] sprintf targets, 6-entry locals written 7-8 times)](notes-unbounded-and-undersized-buffers.md) | abi-64bit | Zero's `Text_UntranscodeName` is the unbounded JP form; whether a real input lacks the 0x50 terminator was not proven |

## All cases by category

### Rendering (38)

| Case | Zero status | Notes |
|---|---|---|
| [Widescreen margins flickered (each rotating frame buffer kept an older scene there)](006-widescreen-margins-flicker.md) | already ported | libultraship fork at plethron's pin 4148f93 has the margin clear |
| [Launcher text blurry on HiDPI (Retina) screens: fonts rasterized at point size](018-launcher-blurry-text-hidpi.md) | already ported | `src/port/launcher.cpp` rasterizes fonts at the window's pixel density |
| [GB Tower: red-orange frames when a Game Boy Color game starts (screen mask ignored in GBC mode)](031-gbtower-gbc-mask-red-flash.md) | not applicable | plethron's `gb_core.c` has no GBC mode; colour carts run in SGB mode |
| [Line across the battle screen: texture rectangles with inverted edges were drawn](033e-texrect-inverted-edges-line-across-screen.md) | already ported | fork at 4148f93, `interpreter.cpp:3962` |
| [OpenGL ES: Poké Ball send-out burst grey and blocky (dual-source blending off)](034e-gles-dual-source-blending.md) | already ported | fork at 4148f93, `gfx_opengl.cpp:251-360` |
| [Smooth motion stretched limbs across the screen after a hitch (blended from a frame many retraces old)](035-interp-no-blend-across-long-gaps.md) | already ported | `src/port/interp.cpp:692-697`, `kMaxGap = 4` |
| [Widescreen: flickering strip at the 2D menus' right edge](037-widescreen-menu-right-edge-flicker.md) | already ported | libultraship fork at pin 4148f93 |
| [Widescreen: bright line at the picture's right edge (2D lock area truncated), fade flicker around widened menus](040-widescreen-2d-lock-rounding-edge-line.md) | already ported | libultraship fork at pin 4148f93; port side in src/port/os_devices.cpp |
| [Menus' rounded boxes: dark lines at their pieces' edges when scaled (texrect samples past a wrapping tile)](046-texrect-edge-clamp-rounded-box-dark-lines.md) | already ported | libultraship fork at pin 4148f93 |
| [Status flashes drew flat silhouettes: fog blend used shade alpha instead of the fog register's alpha](049-fog-blend-by-fog-register-alpha-status-flash.md) | already ported | libultraship fork at pin 4148f93 |
| [Pokédex AREA map: black sea (geo callbacks were MIPS code inside the model blob)](051b-pokedex-area-sea-blob-mips-callbacks.md) | not applicable | Zero's Kanto map callbacks are C code in fragment 030, already lifted |
| [Rectangles fogged by a stale shade alpha (Pokédex AREA label boxes painted in fog colour)](051d-rectangle-fog-blend-stale-shade-alpha.md) | already ported | libultraship fork at pin 4148f93 |
| [RDP NOISE combiner input only covered [0.5, 1) (Sand-Attack's cloud smooth and bright)](056-rdp-noise-range-sand-attack.md) | already ported | libultraship fork at pin 4148f93 |
| [Texgen: lookat kept as floats instead of the RSP's s0.7 bytes (Sing overlay flickers between two looks)](062-texgen-lookat-s07-quantisation-sing.md) | already ported | libultraship fork at pin 4148f93 |
| [Skipped frames ignored render-to-texture into an unshown screen-sized image (Bonus stickers on black)](073-skipped-frames-unshown-screen-image-rtt.md) | already ported | libultraship fork at pin 4148f93; `Port_ViFrameBufferState` in src/port |
| [One black frame after the title: a new frame buffer's first frame was rendered as an off-screen picture](079-black-frame-new-framebuffer-first-frame.md) | needed | the port half is inherited, but Zero's `DisplayTask_Submit` never calls `Port_NoteFrameBuffer` |
| [Window resize blacked out menus that draw their background only once](081-window-resize-keeps-picture.md) | already ported | libultraship fork at plethron's pin 4148f93 has `resize_keep.h` and the backend changes |
| [Rock moves drew a black quad on OpenGL/Vulkan: invisible depth masks kept an opaque blend alpha](087-dual-source-blend-invisible-mask-black.md) | already ported | pinned libultraship GLSL shaders zero the alpha before both output paths |
| [Widescreen: Surf's camera-facing water panel kept its 4:3 width (visible edges in 16:9)](116-surf-water-panel-widescreen.md) | needed | helper `Port_WidenViewSpan` is inherited but Zero's `SurfTerrainGrid_BuildCameraBounds` does not call it |
| [Defeated trainer's portrait vanished: the red X was blended onto an empty GPU render target](117-castle-portrait-red-x-render-target.md) | not applicable | Castle badge carousel only; the libultraship mechanism `gDPPortPreserveColorImage` is inherited |
| [Widescreen: Hypnosis' swirl (camera-facing effect grids) stopped short of the right edge](120a-hypnosis-effect-grid-widescreen.md) | needed | helper `Port_WidenEffectGridX` is inherited; Zero's three grid inits do not call it |
| [Widescreen: screens drawn once (rental list) lost their outer 4 px columns to the margin clear](122b-widescreen-edge-band-on-draw-once-screens.md) | already ported | libultraship fork at 4148f93 clears the edge band only on full repaints |
| [Rental screen: Rapidash's flames used Ponyta's texture offsets (stale cache keyed by host address)](123-rapidash-flames-stale-animated-texture-cache.md) | not applicable | Zero's model-callback table conversions are rebuilt on every call, no cache |
| [Widescreen: a battle stage whose sky is drawn as full-screen rectangles was treated as a 2D screen (boxed in 4:3)](125-elite-four-sky-locked-widescreen-to-4x3.md) | already ported | plethron 6ecd2d8 emits `gDPPort3DScene` in Zero's `Battle_DrawStageBackdrop` |
| [Vulkan backend allocated new command buffers every frame and never freed them (OOM crash after long sessions)](128-vulkan-command-buffer-pool-leak.md) | already ported | libultraship 4148f93 has `AcquireCommandBuffer` |
| [Widened full-screen clear stopped at x=512 on 640x480 screens (stale panels left at the right)](129-fullscreen-fill-margin-640x480.md) | already ported | libultraship 4148f93 scales the margin with the native width |
| [Smooth motion slid recycled effects (sleep Zs, flames, sparkles) towards other objects' places](136b-smooth-motion-matrix-pairing-recycled-slots.md) | already ported | plethron `src/port/interp.cpp` is identical to stadion's current one |
| [Vulkan crashed when the window was minimized (zero-sized swapchain)](137c-vulkan-minimized-zero-swapchain.md) | already ported | libultraship 4148f93 contains fork commit 1a0d7d5 |
| [OpenGL: only the first render-to-texture target got its texture (cup grid crosses missing)](139-opengl-texture-cache-after-framebuffer-create.md) | already ported | libultraship 4148f93 contains fork commit e0a4b28 |
| [Texture rectangles copying the shown frame buffer onto itself painted stale RDRAM (black/noise on OpenGL)](lus-89d2976-framebuffer-self-copy-texrect.md) | check | comes with the libultraship bump; the known trigger is Stadium 1's Gallery, absent in Zero |
| [Metal screenshots showed the previous capture point's frame (or black), not the frame just drawn](lus-a17638c-metal-capture-stale-frame.md) | needed | libultraship bump; plethron's replay/screen-diff tools rely on `PORT_SHOT_AT` captures |
| [Scrolled wrapping textures squashed for a few frames (mipmap-style tile trim applied to a wrapping axis)](lus-bc50ce8-wrap-axis-pyramid-trim.md) | needed | libultraship bump; plethron's pin 4148f93 predates it, and Zero draws the same 32x64 sleep-Z sheet |
| [Memory reused after a render target still served stale GPU pixels (black HUD labels): tell the renderer when a pool block is handed out again](notes-forget-render-targets-on-pool-reuse.md) | needed | the hook exists in libultraship, Zero's allocator never calls it |
| [OpenGL on Linux: shader preprocessor dropped the rest of the shader after a taken @elseif; game picture upside down](notes-opengl-shader-preprocessor-and-orientation.md) | already ported | libultraship fork at 4148f93 |
| [Render-to-texture targets: empty viewports, wrong row/column offsets, passes out of order, markers in RDRAM, images taken for frame buffers](notes-render-to-texture-target-lifecycle.md) | already ported | libultraship fork at 4148f93 and the identical `Port_ViFrameBufferState` in `src/port/os_devices.cpp` |
| [RDP alpha rules the host shaders missed: coverage alpha, invisible depth-only draws, copy-mode alpha compare, transparent-texel bleed](notes-renderer-alpha-coverage-and-invisible-draws.md) | already ported | libultraship fork at 4148f93 |
| [Skipped or dropped frames lost state the N64 keeps across frames (stale segments, missing texture loads, menus missing their one-time backgrounds)](notes-renderer-skipped-frames-keep-rdp-state.md) | already ported | libultraship fork at plethron's pin 4148f93 contains all of it |
| [Caches and "already swapped" flags keyed by address go stale when a file is reloaded at the same address](notes-stale-address-keyed-caches.md) | already ported | descriptor cache cleared with the display-list cache; Zero's swap-once markers are content-based; plethron `0f590be` extends it to replaced model blobs |

### Audio (4)

| Case | Zero status | Notes |
|---|---|---|
| [Steady audio pitch: playback-rate PI control oscillated (wobbling pitch)](047a-audio-playback-rate-control-pitch-wobble.md) | already ported | inherited `src/port/audio_out.cpp` |
| [macOS: sound ~90 ms behind the picture; overdue retraces burst frames into the audio queue](110-macos-audio-lag-and-retrace-bursts.md) | already ported | `src/port/audio_out.cpp` is byte-identical to stadion's; pacing code present |
| [Music / Sound effects sliders followed slider^4 (50 % = -24 dB, 25 % silent)](137a-music-effects-volume-slider-curve.md) | needed | the curve fix is present, but Zero's game never applies the Music/Effects/Voices gains, so those sliders do nothing |
| [GB sound ran 1.5 % slow with host output off: osAiGetLength reported an empty queue](notes-gb-audio-virtual-ai-queue.md) | already ported | `src/port/gb_audio_out.cpp` is identical to stadion's |

### ABI / 64-bit (21)

| Case | Zero status | Notes |
|---|---|---|
| [GB Tower exit crash: emulator staged compressed data ~80 KB past its heap block (pool header clobbered)](019b-gb-emulator-heap-staging-overrun.md) | not applicable | Zero's GB Player copies its emulator data uncompressed into the start of its block; no staging copy past the end |
| [Large Pokémon lost their last graph nodes (Raichu's head): model buffers too small for 64-bit nodes](028-model-buffers-double-size-raichu-head.md) | already ported | `game/src/display_object_textures.c:488`, `MODEL_BUFFER_SIZE (0x3C000 * 2)` |
| [Castle crash at the stadium intro: the stadium file's MIPS entry was called as native code](033d-stadium-intro-blob-called-natively.md) | not applicable | no Stage_LoadModels / stadium intro in Zero; its one blob-entry call already uses Port_CallBlobEntry |
| [Pokédex AREA and Lab PC Items/See List: crashes from N64 struct layouts and undersized buffers](051a-pokedex-lab-n64-layout-buffer-crashes.md) | already ported | Zero's own typing of the same screens |
| [Calls that did not match their functions: stub signatures and a K&R call with an extra argument](052-call-signature-mismatch-stubs.md) | already ported | inherited `src/port/stubs.c`); the minigame call is Stadium-1-only |
| [Victory Palace completion scene crash: a 0xBC0-byte DMA overran its buffer into the next statics](058a-victory-palace-dma-overruns-static.md) | not applicable | Zero has no Victory Palace / completion scene |
| [Lab list filters: PointerList_Filter counted 4-byte pointers (count doubled, garbage rows, crash)](064-pointerlist-filter-count-pointer-size.md) | needed | same `>> 2` in Zero's table_view.c |
| [Lab PC TM/HM list: count as bytes >> 4 and comparator through an N64-offset view (crash opening the list)](068a-lab-tmhm-list-count-and-comparator.md) | already ported | Zero's usable-item list counts entries and compares typed fields |
| [Lab PC list: box picker grid, column header page and "Group by TYPE" read through N64-offset views](068b-lab-pc-list-grids-n64-offset-views.md) | check | Zero's いちらん screens differ; one equivalent bug found, see 070 |
| [Lab PC list: using an item crashed (chosen item read through an N64-offset entry view)](069-lab-pc-item-use-n64-offset-entry-view.md) | already ported | Zero's item-use code reads the typed entry and deck |
| [Lab PC boxes read through an `s32*` view of a pointer list / N64 row stride (Check BOX crash, icons shifted)](070-lab-pc-box-pointer-list-read-through-s32.md) | needed | Zero's `LabBoxList_ApplyChecks` walks the box rows with the N64 stride and offset |
| [Registering a team froze: a 4-entry ring declared as [3] plus N64-sized padding](071-registered-team-ring-four-entries.md) | not applicable | Zero's registration has no 4-entry linked ring |
| [Ditto drew as a flat white blob: its conditional-segment callback cut the display-list pointer to 4 bytes](078-ditto-surface-display-list-truncated-pointer.md) | not applicable | Zero's callback tables are converted to u32 segment words, not host pointers, so the 4-byte read is the whole value |
| [Reset (PRE-NMI) wipe used libultraship's null current-framebuffer stub and detached reset globals](092-prenmi-wipe-null-framebuffer.md) | not applicable | Zero's reset wipe uses its own typed `shownFrame.framebuffer`, not `osViGetCurrentFramebuffer` |
| [Battle crash with tail-flame Pokemon: static flame draw used slot templates before host conversion](131-tail-flame-slot-templates-unconverted.md) | not applicable | Zero's flame templates are typed and converted by the lifter at load, no lazy converter |
| [Code that indexes past one global into the next (N64 link order) reads or writes unrelated host memory](notes-adjacent-globals-out-of-bounds.md) | check | Zero's data is lifted per symbol, sized to the gap to the next N64 symbol; reads before a symbol, e.g. `gMoveData[-1]`, are not covered |
| [OSMesg is 8 bytes on the host: message buffers and osRecvMesg destinations sized for 4-byte messages overflow](notes-message-queue-slot-width.md) | already ported | identical port layer: `_Generic` osRecvMesg and port-owned queue storage |
| [Allocations, clears and strides written as N64 byte counts overflow host structs (Gfx is 16 bytes, pointers 8)](notes-n64-sized-allocations-and-strides.md) | already ported | plethron replaced the literal sizes it found with sizeof / GFX counts; keep auditing new code |
| [Pointers kept in u32/s32 fields, casts and return values are cut to 32 bits on a 64-bit host](notes-pointers-truncated-in-32bit-slots.md) | already ported | Zero's equivalents are widened; strict per-file ratchet in place |
| [Same memory read through two struct types whose fields agree only on the N64 layout (layout checker)](notes-struct-views-with-n64-offsets.md) | already ported | plethron has the checker and typed most views; re-run it after decomp syncs |
| [Buffers the N64 overran harmlessly crash on the host (unterminated GB names, char[1] sprintf targets, 6-entry locals written 7-8 times)](notes-unbounded-and-undersized-buffers.md) | check | Zero's `Text_UntranscodeName` is the unbounded JP form; whether a real input lacks the 0x50 terminator was not proven |

### Endianness (14)

| Case | Zero status | Notes |
|---|---|---|
| [Cup grid showed the wrong trainer portrait (big-endian u16 read by its low byte)](012a-cup-grid-portrait-index-big-endian.md) | not applicable | Zero picks the VS portrait from a host-written u8 `trainerIndex`, not a ROM trainer record |
| [Smoke behind "LOSE" was garbled (table of big-endian segmented addresses read unswapped)](012b-lose-smoke-puff-segment-table-big-endian.md) | not applicable | Zero's WIN/LOSE board has no puff animation reading a ROM address table |
| [Crash after winning a cup: badge award address tables read unswapped and as 8-byte pointers](013-badge-award-address-tables-big-endian.md) | not applicable | Zero's badge award has no ROM address tables; its own badge-award crash was a different bug, fixed in plethron 1b51011 |
| [Gallery rental viewer read its roster unswapped (huge count, byte-swapped stats)](016-gallery-rental-roster-big-endian.md) | not applicable | Zero has no Gallery; its rental rosters are swapped in free_battle_setup_5.c |
| [Castle crash: announcer sound lists were byte arrays in N64 order read as u16 (count read as 768)](033c-announcer-sound-lists-u8-read-as-u16.md) | not applicable | Zero's announcer has no such sound-list tables; data is lifted by type |
| [Geo fog colour sent in memory byte order (pale yellow instead of blue fog)](051c-geo-fog-colour-byte-order.md) | not applicable | Zero stores and emits the fog colour numerically, consistently |
| [Raw big-endian animation headers read natively: endFrame 256x too large (Dream Eater's sparkles missing)](060-raw-animation-endframe-reads.md) | needed | four raw `endFrame` reads remain in Zero's battle code |
| [Rental team picks: a flag tested through an s32 union over four bytes (same Pokémon picked twice)](061a-rental-flag-word-union-byte-order.md) | not applicable | no such union in Zero's Free Battle/rental code; the sweep tool is in plethron |
| [Lab PC list sort by the Pokémon column compared a record's first word as one s32](061b-lab-pc-sort-by-species-group-word.md) | not applicable | Zero's list sorts compare typed fields |
| [Pikachu Surf reward crash: animation in a data fragment read as native (not recognised as raw ROM data)](067a-pikachu-reward-fragment-animation-raw.md) | check | Zero's raw test does not cover segment/fragment-mapped data |
| [Flame column invisible/grey: a runtime N64-to-host converter ran over data the decomp already typed](142b-typed-data-converted-twice.md) | not applicable | plethron has no runtime template converter; the lifter types the flame tables once |
| [ROM and blob data are big-endian: tags, headers, records, descriptors and byte lists read natively on the host](notes-big-endian-rom-data-read-natively.md) | already ported | the asset layer is "from stadion"; Zero's data symbols are lifted per type with swapping |
| [Unions that overlay a narrow field on a wider integer read the wrong half on a little-endian host (animation frame counter, GB APU phase)](notes-narrow-fields-through-wide-unions.md) | needed | Zero's `AnimFixedFrame` and `GbApuChannelState.phase` still use the N64 member order |
| [Blank screen between scenes: the one-line frame buffer must be written in N64 byte order (dark green instead of black)](notes-vi-blank-line-n64-byte-order.md) | needed | Zero's `Display_ClearFramebufferLine` writes host order |

### Input (2)

| Case | Zero status | Notes |
|---|---|---|
| [Only controller 1 was presented to the game (every multiplayer choice greyed out)](075-controllers-2-4-presented-to-game.md) | needed | port side inherited, but Zero's `Cont_SetupControllers` does not use it |
| [Intermittent frame-0 crash: osContInit on the game thread raced the window thread's pad polling](098-controller-init-on-window-thread.md) | already ported | `port_osContInit` marshals to the window thread; Zero calls `osContInit` through the macro |

### Saves (4)

| Case | Zero status | Notes |
|---|---|---|
| [Launcher: Transfer Pak "Change" save button opened Save As instead of picking an existing file](089-gb-save-picker-opened-save-as.md) | already ported | plethron's launcher passes `false` for the GB save picker |
| [Windows: settings and save files under non-ASCII paths (narrow fopen/ifstream on UTF-8 paths)](103-unicode-config-and-save-paths.md) | needed | save/config paths are fixed, but plethron's own `gb_save_convert.cpp` still opens its store with plain `fopen` |
| [Save editor could not enter Game Boy special characters in names (forced upper case, no gender signs, PK/MN)](137b-save-editor-gb-special-characters.md) | already ported | plethron's save editor has the tokens and Symbols picker |
| [Saves: wrong flash status constants dropped every page after the first; save image written in host byte order](notes-flash-status-and-save-byte-order.md) | not applicable | Zero saves to SRAM, not FlashRAM); the byte-order half is already ported (plethron `ec82138` |

### Platform / build (23)

| Case | Zero status | Notes |
|---|---|---|
| [macOS crash at start: Metal shaders lost the `thread` qualifier on reference parameters](005-metal-shader-thread-qualifier-on-references.md) | already ported | plethron's `tools/make_o2r.py:30` has stadion's narrowed regex |
| [Windows build broke: a local named `far` in the margin clear (windows.h macro)](015-windows-build-far-macro.md) | already ported | fork at 4148f93 uses `reach` |
| [Launcher crashed when hovering Smooth motion (ImGui multi-viewport tooltip window on Metal)](024-launcher-tooltip-crash-multiviewports.md) | already ported | `src/port/Engine.cpp:249-251`, `src/port/launcher.cpp:887` |
| [Android: status and navigation bars covered the picture (window not fullscreen)](034d-android-fullscreen-system-bars.md) | already ported | `src/port/Engine.cpp:253-258` |
| [DXGI/DX11: startup crash from uninitialised window-backend and renderer members](042a-dxgi-uninitialised-window-members-startup-crash.md) | already ported | libultraship fork at pin 4148f93 |
| [Windows: crashes after startup left no log (port's exception filter replaced libultraship's)](042b-windows-port-crash-handler-swallowed-libultraship-log.md) | already ported | inherited port layer |
| [Wrong ROM dumps started and crashed mid-game: check the ROM's SHA-1](045-rom-sha1-check-wrong-dump-crashes.md) | already ported | rom_check adapted to Zero's Japanese ROM |
| [Windows crash 0x20474343 at start with a damaged or missing pokestadium.o2r: archive built into the exe](082-builtin-o2r-archive-damaged-file-crash.md) | already ported | plethron mounts the built-in archive `gPortO2r` |
| [Vulkan: a failed swapchain creation crashed in the driver instead of falling back](084-vulkan-swapchain-failure-crash.md) | already ported | pinned libultraship checks swapchain calls; plethron main.cpp catches `Fast::VulkanFatalError` |
| [Windows: ROM under a non-ASCII path could not be validated or loaded (narrow fopen on a UTF-8 path)](088-windows-utf8-rom-paths.md) | already ported | `Port::OpenFileUtf8` is used for the ROM check and the loader |
| [Windows: fragment data/BSS were not reset when a fragment was loaded again (stale hub fade)](090a-windows-fragment-bss-reset.md) | already ported | plethron's `tools/gen_frag_sections.py` emits `.data$fdNN$`/`.bss$fbNN$` sections |
| [GB Tower: cartridge detection walked backwards from one title array into another (host linkers reorder data)](090b-gb-cart-detect-adjacent-title-arrays.md) | not applicable | Zero's `GbPak_IdentifyCartridge` walks forward inside one 96-byte table |
| [Windows launcher was blurry/small on high-DPI displays (no DPI awareness)](105-windows-launcher-dpi-blurry.md) | already ported | plethron's manifest declares PerMonitorV2; libultraship DXGI fix is at the pin |
| [Launcher Save editor button did nothing in packaged apps (missing tools dir, Finder PATH)](122c-launcher-save-editor-packaged-app.md) | not applicable | plethron's launcher uses the built-in C++ save editor, no Python or tools folder |
| [Vulkan: driver out-of-memory crashed in the next call; now falls back to OpenGL](124-vulkan-out-of-memory-fallback.md) | already ported | pinned libultraship throws `Fast::VulkanFatalError`; plethron main.cpp catches it |
| [stadium_save_cli on Windows: non-ASCII arguments (♂, é) arrived as "?"](130-save-cli-utf8-arguments-windows.md) | already ported | plethron `src/port/save_editor/cli_main.cpp` reads the wide command line |
| [Windows release exe kept its COFF symbol table (read as an overlay, flagged by an AV engine)](133-windows-exe-symbol-table-av-flag.md) | already ported | plethron `tools/release.sh` and `build.yml` use `--strip-all` |
| [Windows build: wingdi.h's Rectangle() collided with the game's Rectangle type](142a-windows-nogdi-rectangle-collision.md) | already ported | plethron `CMakeLists.txt:283-286` defines NOGDI for `stadium_game` |
| [Byte-order helpers were static copies per file: other files hit "call to undeclared function"](148-duplicated-static-byte-swap-helpers.md) | already ported | `game/include/port_endian.h`, duplicate check in `tools/check_port_decls.py` |
| [Android APK build failed: NDK's -Werror=format-security not undone by the game's -Wno-error](153-android-format-security-werror.md) | check | flag missing in plethron, but no triggering call found in Zero's game code today |
| [macOS-only assumptions: "N64 address = below 4 GB", 47-bit vertex check, Mach-O symbol prefixes and sections](notes-low-address-heuristics-and-symbol-prefixes.md) | already ported | `Port_ReserveLowAddressSpace`, 48-bit check in the fork, per-platform symbol and section code |
| [Shifts by 32 or by a negative count: MIPS takes the count modulo 32, C on x86/arm64 does not (JPEG bit reader)](notes-mips-shift-count-modulo-32.md) | already ported | `game/src/jpeg_decoder.c:196-199`, "from stadion" |
| [Closing the window crashed: main returned while game threads still ran, static destructors destroyed a mutex in use](notes-window-close-static-destructors.md) | already ported | `src/port/main.cpp:522-528` |

### Game logic (12)

| Case | Zero status | Notes |
|---|---|---|
| [Quitting the Game Boy emulator closed the game: osDestroyThread(NULL) returned](019a-osdestroythread-self-returned.md) | already ported | `src/port/os_thread.cpp:247-254`; Zero calls `osDestroyThread(NULL)` in gb_emu_draw.c:330 |
| [Gym Leader Castle crash: Battle_EstimateMoveDamage read past a move table the decomp had split in three](033b-move-table-split-overread.md) | not applicable | Zero's announcer does not use Battle_EstimateMoveDamage or this table |
| [Battle AI crash (unoptimized builds): `ptr->unk_57 * 0` read through an unset pointer](034f-battle-ai-read-through-unset-pointer.md) | not applicable | Zero's COM AI has no BattleAI_SelectBestScoredMove; no `ptr->... * 0` pattern |
| [Hyper Beam / Ice Beam strands lost their wave: a never-written local folded to 0 by clang](055-beam-strands-uninitialised-stack-phase.md) | needed | same function, same unset `spA8` |
| [Badge carousel after a Free Battle: uninitialised archive index read a wild trainer record](058b-badge-carousel-uninitialised-archive-index.md) | not applicable | Zero has no battle_prep badge carousel |
| [Game threads must use the FPU rounding mode the game sets (__osSetFpcCsr was a stub)](059-fpu-rounding-mode-from-fpcsr.md) | already ported | inherited `src/port/os_devices.cpp`; Zero asks for round-to-nearest, so it is a no-op there |
| [Event Battle with a short party: species 0's name read record -1 (wild pointer)](067b-species-name-of-empty-slot-wild-pointer.md) | already ported | guard present in Zero's `Text_CopySpeciesName` |
| [Gallery prompts drawn as empty boxes: string getters fell off the end without returning](072-gallery-common-ui-string-missing-return.md) | not applicable | no Gallery in Zero; the bug class is audited in `074-...` |
| [Non-void game functions that fall off the end: the host returns garbage where the N64 left v0 (VS Mewtwo intro skipped)](074-functions-falling-off-the-end-v0.md) | check | 34 such functions in Zero; most results unused, three audio/GB ones need a look |
| [COM AI read an uninitialized selected-move index for a fainted Pokemon (out-of-bounds crash)](086-com-ai-fainted-uninitialized-move-index.md) | not applicable | Zero's COM AI has no per-candidate move-score records or `BattleAI_ScoreAttackVsSwitch` |
| [Castle Champion transition crash: location 9 indexed one past a nine-entry texture table](100-champion-banner-table-overrun.md) | not applicable | Gym Leader Castle / Elite Four / Champion are Stadium 1 only |
| [Statically linked fragments kept their globals between visits (area select came back black)](notes-fragment-static-data-reset.md) | already ported | `Port_FragmentResetData` called from Zero's fragment loader |

### Timing / pacing (12)

| Case | Zero status | Notes |
|---|---|---|
| [Uneven frame pacing: free-running VI clock woke late and beat against the display refresh](021a-retraces-beat-against-display.md) | already ported | `RetraceSchedule` in `src/port/os_devices.cpp:957`, `src/port/pacing_mac.mm` |
| [Vsync stayed off after a PORT_SPEED run (saved into the config)](021b-vsync-off-saved-by-speed-runs.md) | already ported | `src/port/Engine.cpp:213-228` |
| [Android stutter: retraces on a free clock drifted against the panel (59.35 Hz), frames landed on uneven refreshes](034a-android-retraces-follow-vsync.md) | already ported | `src/port/pacing_android.cpp` |
| [Windows: game clock slept on the 15.6 ms timer tick (retraces in bursts, audio ran short)](047b-windows-coarse-sleep-timer-1ms.md) | already ported | inherited `src/port/pacing.cpp` |
| [Boot intro out of sync with its music: the N64's synchronous model loads were instant](048-boot-diorama-n64-load-waits-av-sync.md) | not applicable | Zero has no boot diorama; the port-side switch exists but nothing calls it |
| [N64 logo held by a busy-wait on osGetTime: no frames presented for a second (frame gap at boot)](106b-n64-logo-busy-wait-hold.md) | check | Zero has the same busy-wait, `N64Logo_HoldForOneSecond`; decide whether the gap shows on Zero's targets |
| [Slow GPUs: game time fell behind real time (slow motion, sound queue starving)](118-slow-gpu-game-slows-and-sound-stutters.md) | already ported | lag make-up and frame skipping are in plethron's os_devices.cpp |
| [Smooth motion: menus stayed at 30 fps because setting the same segment map flushed the DL cache every frame](120b-segment-map-flush-dropped-inbetween-frames.md) | already ported | plethron 0f590be: `Memmap_SetSegmentMap` flushes only on a change |
| [Smooth motion on slow GPUs froze then jumped ahead (in-between pictures did not fit a game frame)](136a-smooth-motion-picture-budget-freezes.md) | already ported | plethron `src/port/interp.cpp` is identical to stadion's current one |
| [GB Tower Doduo/Dodrio fast-forward had no effect (native GB core always ran one frame)](140-gb-tower-fast-forward-ignored.md) | already ported | Zero's GB Player loops the core 1/2/4 times on the game side; plethron `bb27580` |
| [Smooth motion picture budget misfired: 2160p menus and slow GPUs held at one picture per game frame](141-smooth-motion-budget-misfires.md) | already ported | plethron `src/port/interp.cpp` is identical to stadion's current one |
| [Graphics tasks rendered asynchronously read display-list memory the scene had already freed](notes-rsp-task-submit-waits-for-render.md) | already ported | `osSpTaskStartGo` waits for the window thread in the shared `src/port/os_devices.cpp` |

### Performance (17)

| Case | Zero status | Notes |
|---|---|---|
| [Metal hitches: every new shader built a pipeline for every MSAA level](012c-metal-pipelines-built-lazily-per-msaa.md) | already ported | libultraship fork at 4148f93 has `PipelineFor` |
| [~0.35 s pause at boot and every scene change (window loop polled RSP tasks with a 1 ms sleep)](014-scene-change-pause-rsp-task-wait.md) | already ported | `Port::WaitForRspTasks` in src/port |
| [CPU wasted in getenv: debug switches read on every call in per-frame/per-command code](029-getenv-per-call-hot-paths.md) | already ported | static caches in src/port and the fork; `PORT_GETENV_ONCE` in `src/port/StadionPort.h:179` |
| [Memory leak: translated display lists never freed after a cache flush (~15 MB/s in battle)](033a-translated-display-list-arena-leak.md) | already ported | `src/port/gfx_translate.cpp:44-60`, `:571-579` |
| [Android: ~15% CPU in the unwinder capturing stacks on every blocking receive](034b-android-stack-capture-unwinder-cpu.md) | already ported | `src/port/port_platform.h:93-117`, `src/port/os_thread.cpp:388` |
| [Texture cache: rewritten RAM buffers piled every version into one bucket chain (window-thread CPU)](034c-texture-cache-content-hash.md) | already ported | fork at 4148f93: content hash in `ImportTexture`, interpreter.cpp:1862 |
| [Texture cache keyed by content: RAM textures re-uploaded every frame (battle stutter)](036-texture-cache-keyed-by-content.md) | already ported | in the libultraship fork at plethron's pin 4148f93 |
| [First Poke Ball throw stutter: fogged translucent shader programs were missing from the warm-up list](080-shader-warmup-fogged-translucent-programs.md) | check | the list is inherited byte-identical, but it holds Stadium 1's programs; Zero's own scenes need their own recorded list |
| [Startup stutter: shader warm-up still running while the intro played; now finished before the intro](106a-shader-warmup-before-intro.md) | already ported | plethron main.cpp runs `Port::RunShaderWarmup()` before `Main()` |
| [First-entry save stall: every FlashRAM page write rewrote the save file (with backups)](106c-flash-save-per-bank-write-stall.md) | check | Zero saves to SRAM; plethron writes the whole .sra with a backup snapshot on every changed DMA write |
| [Vulkan stutter at first battle/scene: pipeline states built at first draw (warm-up list)](108-vulkan-pipeline-warmup-first-battle.md) | check | the list file is inherited byte-identical; its entries come from Stadium 1 recordings |
| [Pauses at screen changes: 300 small JPEG tasks each round-tripped through the window thread](122a-jpeg-tasks-on-game-thread.md) | already ported | plethron's `osSpTaskStartGo` runs JPEG tasks on the submitting thread |
| [Lockstep clock polled for "game idle" every 20 us (~28k wake-ups/s, more kernel than game time)](152a-lockstep-clock-idle-polling.md) | needed | plethron's `LockstepClock` still sleeps 20 us in a polling loop |
| [Window loop pumped OS events and polled pads on every ~1 ms pass (85 % of the window thread's time)](152b-event-pump-every-loop-pass.md) | needed | plethron `PumpFrame` pumps unconditionally; see also `157-...` |
| [Event pump's 4 ms floor still cost twice the game's drawing; replaced by 33 ms floor + mid-frame pump](157-event-pump-4ms-floor.md) | needed | depends on `152b-...`, also missing |
| [Each render-to-texture readback waited for the GPU on its own (up to 8 ms hitches at scene changes)](lus-003614f-rtt-readback-batch.md) | needed | libultraship bump; Zero's VS screen and portraits use off-screen targets |
| [Texture cache key hashed every imported texture one byte at a time (FNV-1a, ~19 us per 4 KB)](lus-5a3431c-texture-hash-8-bytes.md) | needed | libultraship bump; pin 4148f93 has the byte loop |

<!-- END INDEX -->

## stadion PRs with no case

These were features, tooling, CI, docs or branding, or never merged: #1-4, #7-11, #17, #20, #22, #23, #25-27, #30, #32, #38, #39, #41, #43, #44, #50, #53, #54, #57, #63, #65, #66, #76, #77, #83, #85, #91, #93-97, #99, #101, #102, #104, #107, #109 and #111 (folded into 108), #112, #113-115, #119, #126, #127, #132, #134, #135, #138, #143-145, #147, #149-151, #154-156 (#155/#156 are covered by the `lus-*` cases).
