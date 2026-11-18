#ifndef POKEDEX_NAME_ORDER_H
#define POKEDEX_NAME_ORDER_H

#include "ultra64.h"

// PORT: the Pokédex index's アイウエオじゅん (kana order) sorted by the names in the translated language
// (game/src/pokedex_name_order.c).

// Whether the kana order is replaced: a translated language other than Japanese, and the port setting
// gPort.PokedexNameOrder (on by default).
s32 PokedexNameOrder_Active(void);

// The dex number at position `pos` (1-based) of the translated name order, or 0 when it is not active.
s32 PokedexNameOrder_Entry(s32 pos);

#endif
