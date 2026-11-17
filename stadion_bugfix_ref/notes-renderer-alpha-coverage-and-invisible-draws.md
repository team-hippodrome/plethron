# RDP alpha rules the host shaders missed: coverage alpha, invisible depth-only draws, copy-mode alpha compare, transparent-texel bleed

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at 4148f93)
- **stadion:** no PR (Renderer round, third ares round VS screen and Kids Club, GB Tower menu; before PR #1, history squashed into `be110fe`). The OpenGL/Vulkan dual-source mask fix is PR #87 (covered separately).
- **stadion files:** libultraship `src/fast/interpreter.cpp`, `src/fast/backends/gfx_metal.cpp`, `gfx_opengl.cpp`, `gfx_direct3d11.cpp`, `default.shader.*`

## Symptom
- White boxes behind the order screen's icons and pointer (copy mode with alpha compare).
- Sandshrew's Dig: big black hexagons over the ground (depth-only "invisible" mask drawn in its black colour).
- VS screen: missing leaves, petals, tail flames; the Game Pak WARNING stripes gone; opaque black cell backgrounds.
- HUD digits and "HP:" pink-tinged above native resolution (magenta key colour of transparent texels bled by bilinear filtering); then GB Tower menu text black/lilac (bleed applied to point-sampled CPU frames).
- Paletted models drawn with TLUT off (Yellow's Pikachu) showed garbage.

## Root cause
On the N64 the colour image's alpha bit is coverage (any drawn pixel); blending used to keep the destination alpha. Copy mode with alpha compare drops texels with alpha bit 0 even without blending. A 2-cycle blender `IN*0 + MEM*1` is invisible whatever B is. Bilinear filtering on the GPU mixes the RGB of fully transparent texels, which the N64 at native size never shows.

## Fix in stadion
Dual-source blending (RGB by texel alpha, alpha written as 1, 0 for invisible draws); `use_alpha` forced for copy mode with alpha compare; invisible draws always blend; `ClearFramebufferColor` with the fill colour's alpha bit; `BleedTransparentTexels` for RGBA16/32 and CI uploads, only for filtered sampling; CI with TLUT off samples the index as intensity. Debug: `PORT_OLD_ALPHA_BLEND=1`, `PORT_NO_ALPHA_BLEED=1`, `PORT_NO_RTT_FILL_ALPHA=1`, `PORT_GL_NO_DUAL=1`.

## Zero / plethron
Already ported via the fork at 4148f93: `src/fast/interpreter.cpp:2704-2742` (invisible draws), `:1044` (alpha bleed switch), `:4234` (RTT fill alpha), `src/fast/backends/gfx_metal.cpp:402` (old alpha blend switch), `gfx_opengl.cpp:360-361` (`dual_out`). Nothing game-side.

## How to detect
Compare against ares at the same frame; translucent texels missing on render-to-texture images; black or coloured fringes on cut-out UI art at 2x+; `PORT_DRAWLOG=1` shows the render mode of the offending draw.
