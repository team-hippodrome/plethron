// Hooks the battle calls for the enhancements (battle_enhancements.c). Each does nothing while the enhancements are
// off.
#pragma once
#include "global.h"

void PortEnh_BattleBegin(void);          // BattleScene_DispatchLifecycle(0): a battle allocates its participants
void PortEnh_BattleFrame(void);          // BattleScene_DispatchLifecycle(5): after the battle HUD, every frame
void PortEnh_TurnOrder(s32 first_side);  // Battle_StartTurn, once Battle_DetermineMoveOrder picked who acts first;
                                         // -1 from BattleScene_InitializeSwitchActions (a side switches, no order)
void PortEnh_MoveAttempted(void);        // Battle_ProcessTurn: the acting side can act, before the move resolves
void PortEnh_TransformShown(void);       // BattleScene_PhaseTransform: the copied model is loaded

// The summary bars on the order screen (order_preview.c): BattleMenu_UpdateOrderSelect calls OrderFrame every frame
// after drawing its panels, BattleMenu_RunOrderSelectScreen calls OrderEnd when the screen closes.
struct OrderSelectPanel;
void PortEnh_OrderFrame(struct OrderSelectPanel* panels, s32 count);
void PortEnh_OrderEnd(void);

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

// Hue choice (hue_picker.c). PortEnh_HueShift: the hue to build `mon`'s model and icon with, `game_shift` (the
// game's own, from its nickname) unless the player picked one (Model_ComputeNicknameColorVariant). The picker on the
// entry screen's Pokémon cards: Input steps the hue back / forward or resets it on those buttons of `pressed` and
// returns 1 when it changed (the caller reloads the model and the icons); Draw puts the current hue on the bottom edge
// of the model's panel (x, y: the panel's bottom left, w: its width); CardHint fills the card's prompt strip with the
// colour card's buttons.
s16 PortEnh_HueShift(BattleMon* mon, s16 game_shift);
s32 PortEnh_HueBypassed(void);
s32 PortEnh_HuePickerInput(BattleMon* mon, u16 pressed, u16 back, u16 forward, u16 reset);
// The stick's left / right tilt (Controller.stickX, 0 inside the dead zone) turns the hue finely; 1 when it changed.
s32 PortEnh_HuePickerStick(BattleMon* mon, f32 stick_x);
void PortEnh_HuePickerDraw(s32 x, s32 y, s32 w, BattleMon* mon);
void PortEnh_HueCardHint(s32 x, s32 y, s32 w, s32 h, s32 party);

// The battle command menus' enhancements (battle_menus.c): mid-battle move descriptions and order queueing.
struct ActivePokemon;
void PortEnh_BattleMenusBegin(void);                                 // from PortEnh_BattleBegin
void PortEnh_BattleMenusFrame(void);                                 // from PortEnh_BattleFrame, every HUD frame
void PortEnh_MoveMenuInput(struct ActivePokemon* a);                        // BattleMenu_UpdateMoveMenu, before its C buttons
void PortEnh_PlayerCommandBegin(struct ActivePokemon* a, struct ActivePokemon* foe); // BattleScene_CollectPlayerAction, first
void PortEnh_PlayerCommandEnd(struct ActivePokemon* a);                     // and last

// Stat Exp difficulty (statexp_difficulty.c): applies the rental (PORT_SE_RENTAL) or opponent (PORT_SE_OPPONENT)
// setting to `count` Pokémon's stat exp and works out their stats again; nothing while that setting is Vanilla.
// `what` names them in the PORT_ENH_LOG=1 log.
#define PORT_SE_RENTAL 0 // (as in enhancements.h)
#define PORT_SE_OPPONENT 1
void PortEnh_StatExpDifficultyTeam(BattleMon* mons, s32 count, s32 group, const char* what);
// Tournaments: the COM team just loaded (opponent setting) and 1P's rentals (rental setting, from Zero's own stat exp).
struct TeamRoster;
struct ExtendedRosterInfo;
void PortEnh_StatExpDifficultyTournament(struct TeamRoster* com, struct ExtendedRosterInfo* comInfo);
// The opponent clamp as one range (hu_se_bounds; 0..65535 = Zero's own stat exp), for the Speed estimate
// (SPEED_STAT_ESTIMATE: estimates are kept with the clamp they were learned under). `forThisBattle`: 0..65535 outside
// tournaments (Free Battle is never changed); 0: the setting itself (trainer memory screen, tournament trainers only).
void PortEnh_OpponentStatExpBounds(s32 forThisBattle, u16* lo, u16* hi);
