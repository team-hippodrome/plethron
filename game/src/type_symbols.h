#ifndef TYPE_SYMBOLS_H
#define TYPE_SYMBOLS_H

#include "ultra64.h"

/* PORT: not part of the decomp. The battle move panel's type labels (type_symbols.c). */
void TypeSymbols_DrawMoveTypeLabel(s32 x, s32 y, s32 type, u8* original);
/* PORT: the switch menu's status stamps (index of D_8218AFDC: 1 fainted, 2 sleep ... 6 paralysis). */
void TypeSymbols_DrawStatusLabel(s32 x, s32 y, s32 status, u8* original);

#endif
