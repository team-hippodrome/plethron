// Hooks the battle calls for the enhancements (battle_enhancements.c). Each does nothing while the enhancements are
// off.
#pragma once
#include "global.h"

void PortEnh_BattleBegin(void);          // BattleScene_DispatchLifecycle(0): a battle allocates its participants
void PortEnh_BattleFrame(void);          // BattleScene_DispatchLifecycle(5): after the battle HUD, every frame
void PortEnh_TurnOrder(s32 first_side);  // Battle_StartTurn, once Battle_DetermineMoveOrder picked who acts first
void PortEnh_MoveStart(void);            // a move's presentation starts (BattleScene_ActionStepSetHitCount)
void PortEnh_MoveNotUsed(void);          // Battle_ProcessTurn: the acting side cannot act (sleep, paralysis, flinch...)

// The trainer memory screen on the tournament menus (memory_screen.c), from FreeBattle_UpdateScreen /
// FreeBattle_DrawScreen. `state` is the battle setup screen's state (7 the Lv. 50-55 menu, 9 the Lv. 1-30 cup
// select), `cup_cursor` the highlighted cup. Update returns 1 while the screen is open (it takes the controller).
s32 PortEnh_MemoryScreenUpdate(s32 state, s32 cup_cursor);
void PortEnh_MemoryScreenDraw(s32 state, s32 cup_cursor);
// From the N64 logo (N64Logo_Main), while it holds its last frame: reads the memory file and builds the screen's
// glyphs on a background thread, so the first open does not wait for them.
void PortEnh_MemoryScreenPrewarm(void);
// A move's type as hippodrome-utils numbers it (hu_type), for the move names' colours.
u8 PortEnh_MoveHuType(u16 move);
