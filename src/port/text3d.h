// Translated words for text the game draws as 3D models (the VS screen's round titles, かち / まけ on the results
// panels). The game's letters are untextured, lit, bevelled meshes, one display list per glyph, each under its own
// animated model part. In a translated language the port builds the translation (translations/*.csv, MODEL_TEXT_*)
// from the outlines in assets/font/text3d_subset.otf with the same bevel profile, splits it over the word's glyph
// parts and swaps the meshes in where Port_GfxPrepareDL resolves the glyphs' display lists. Nothing changes in the
// original language.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One glyph of the original word: its display list's link address and where its part sits at rest, in the space of
// the model the word belongs to (the sum of the default translations of the animated parts above it, scaled by the
// parts above those, and the product of their scales).
typedef struct Port3DTextSlot {
    uint32_t dl;
    float x, y, z;
    float scaleX, scaleY, scaleZ;
} Port3DTextSlot;

// Whether the current language translates 3D words at all (0 in the original kana).
int Port_Text3D_Enabled(void);

// Builds the translation of row `id` over `slots` (in reading order of the original word) and registers the
// replacement display lists. `maxGrow` is how much wider than the original word the translation may be before it is
// squeezed. Does nothing when the language is the original or the row has no text in it.
// `followers` (may be NULL) are original parts drawn after the word that stay as they are (the round title's dash
// and number): the word then takes their height, and they move right by as much as the word grew; `maxGrow` then
// limits the whole line.
void Port_Text3D_Build(const char* id, const Port3DTextSlot* slots, int count, float maxGrow,
                       const Port3DTextSlot* followers, int followerCount);

// Port_GfxPrepareDL: the replacement list for the glyph display list at link address `dl`, or NULL.
void* Port_Text3D_Replacement(uintptr_t dl);

// Forgets the replacements (the scene that owns the glyphs is ending). Their memory is kept until the next Build,
// since frames still queued for the renderer may use it.
void Port_Text3D_Clear(void);

#ifdef __cplusplus
}
#endif
