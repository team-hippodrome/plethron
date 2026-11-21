#pragma once
// The summary bars' 3D models (card_models.c, game code).
#include "global.h"
#include <hippodrome/summary_card.h>

// Frees every copy (a new battle).
void PortEnh_CardModelsFree(void);
// Loads card `card` of `side`'s copy of `mon`'s model (nothing if that species is already there).
void PortEnh_CardModelsLoad(s32 side, s32 card, BattleMon* mon);
// While transformed: card `card` shows the model of (`from_side`, `from_card`); Restore puts its own back.
void PortEnh_CardModelsLoadFrom(s32 side, s32 card, s32 from_side, s32 from_card);
void PortEnh_CardModelsRestore(s32 side, s32 card);
// Whether the card's model is loaded and the picture exists.
s32 PortEnh_CardModelReady(s32 side, s32 card);
// Draws the copies into the port's card picture (after the battle's HUD), animated as the cards' states say.
// active[s]: the card in battle on side s (-1 none); battle[s]: that side's battle model
void PortEnh_CardModelsDraw(const hu_side sides[2], const s32 active[2], DisplayObject* const battle[2]);
